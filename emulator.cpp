#include "Arduino.h"
#include "emulator.h"

#define STACK_ALIGNMENT 16
#define START_ADDR 0x400

// configures mini-rv32ima.h
#define MINIRV32_DECORATE
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_STEPPROTO MINIRV32_DECORATE int32_t Emulator::MiniRV32IMAStep( struct MiniRV32IMAState * state, uint8_t * image, uint32_t elapsedUs, int count )
#define MINI_RV32_RAM_SIZE this->_ramSize
#define MINIRV32_RAM_IMAGE_OFFSET 0x0

// #define MINIRV32_POSTEXEC(pc, ir, trap) this->dumpState(state, image, pc);

#include "mini-rv32ima.h"


Emulator::Emulator(uint8_t* ram, uint32_t ramSize) {
	_ram = ram;
	_ramSize = ramSize;
  _state = STATE_NORMAL;
  memset(&_blockedState, 0, sizeof(BlockedState));
  memset(&_files, 0, sizeof(_files));

  // initialize RAM, registers
	memset((void*) &_m, 0, sizeof(MiniRV32IMAState));
  _m.regs[2] = (START_ADDR & 0xfffffff0) - 16;
  _m.pc = START_ADDR;
}

Emulator::~Emulator() {
  Serial.println("destructor called");
  for(uint8_t i = 0; i < MAX_FDs; i++) {
    switch(this->_files[i].type) {
      case FILE_UNALLOCATED:
        break;
      case FILE_SERVER:
        delete this->_files[i].data.server;
        break;
      case FILE_CLIENT:
        this->_files[i].data.client->stop();
        delete this->_files[i].data.client;
        break;
    }
    this->_files[i].type = FILE_UNALLOCATED;
  }
}

EmuState Emulator::state() {
  return this->_state;
}

void Emulator::step(int numSteps) {
  switch (this->_state) {
    case STATE_NORMAL:
      if (this->noTrap()) {
      	MiniRV32IMAStep(&this->_m, (uint8_t*) this->_ram, millis(), numSteps);
        // dumpState(&this->_m, (uint8_t*) this->_ram, this->_m.pc);
      }

      // Handle any fault or exception.
      switch (this->_m.mcause) {
        case 0x08: // ECALL (syscall) - U-Mode
          handleSyscall();
          break;
      }
      break;

    case STATE_BLOCKED:
      switch (this->_blockedState.reason) {
        case BLOCKED_DELAY_TILL:
          if (millis() >= this->_blockedState.delayTill) {
            this->_state = STATE_NORMAL;
          }
      }
      break;

    case STATE_EXIT_OK:
    case STATE_EXIT_ERR:
      break;
  }
}

// guestAddrLenOOB returns a non-zero value if the memory region specified exceeds the guest RAM.
uint32_t Emulator::guestAddrLenOOB(uint32_t baseAddr, uint32_t size) {
  return baseAddr > this->_ramSize || baseAddr+size > this->_ramSize;
}

bool Emulator::noTrap() {
  return this->_m.mcause == 0;
}

uint32_t Emulator::findFreeFd() {
  for(uint8_t i = 0; i < MAX_FDs; i++) {
      if (this->_files[i].type == FILE_UNALLOCATED) {
        return i;
      }
    }
  return -1;
}

void Emulator::dumpState( struct MiniRV32IMAState * core, uint8_t * ram_image, uint32_t pc )
{
  Serial.flush();

	uint32_t pc_offset = pc - MINIRV32_RAM_IMAGE_OFFSET;
	uint32_t ir = 0;

	Serial.printf( "PC: %08x ", pc );
	if( pc_offset >= 0 && pc_offset < MINI_RV32_RAM_SIZE - 3 )
	{
		ir = *((uint32_t*)(&((uint8_t*)ram_image)[pc_offset]));
		Serial.printf( "[0x%08x]", ir ); 
	}
	else
		Serial.printf( "[xxxxxxxxxx]" ); 

  Serial.print(" -- ");
  switch (this->_state) {
    case STATE_NORMAL:
      Serial.print("RUN");
      break;
    case STATE_EXIT_OK:
      Serial.print("EXIT OK");
      break;
    case STATE_EXIT_ERR:
      Serial.print("EXIT ERR");
      break;
  case STATE_BLOCKED:
        Serial.printf("BLOCKED %x", this->_blockedState.reason);
        break;
  }
  Serial.print("\r\n");
   
	uint32_t * regs = core->regs;
	Serial.printf( "\tra:%08x sp:%08x gp:%08x tp:%08x t0:%08x t1:%08x t2:%08x s0:%08x s1:%08x\r\n\ta0:%08x a1:%08x a2:%08x ",
		regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
		regs[8], regs[9], regs[10], regs[11], regs[12]);
  Serial.printf( "a3:%08x a4:%08x a5:%08x a6:%08x a7:%08x\r\n",
    regs[13], regs[14], regs[15], regs[16], regs[17]);

  Serial.printf("\tmstatus=%08x mcause=%08x mtval=%08x\r\n", core->mstatus, core->mcause, core->mtval);
}


void Emulator::handleSyscall() {
  uint8_t fault = 0;
  uint32_t retval = this->_m.regs[17]; // syscall number in x10
  
  switch (retval) { // syscall number
    case 0: // exit <exit-code>
      if (this->_m.regs[10]) {
        this->_state = STATE_EXIT_ERR;
      } else {
        this->_state = STATE_EXIT_OK;
      }
      break;

    case 1: // print string of specified length
      {
        uint32_t addr = this->_m.regs[10];
        uint32_t len = this->_m.regs[11];
        fault = guestAddrLenOOB(addr, len);

        if (fault) {
          retval = -1;
        } else {
          void* base = this->_ram + addr;
          Serial.printf("%.*s", len, (const char*) base);
        }
      }
      break;

    case 2: // compare strings
      {
        uint32_t addr1 = this->_m.regs[10];
        uint32_t addr2 = this->_m.regs[11];
        uint32_t len = this->_m.regs[12];
        fault = guestAddrLenOOB(addr1, len) + guestAddrLenOOB(addr2, len);

        if (fault) {
          retval = -1;
        } else {
          retval = strncmp((const char*) (this->_ram+addr1), (const char*) (this->_ram+addr2), len);
        }
      }
      break;

    case 3: // memset
      {
        uint32_t addr = this->_m.regs[10];
        uint32_t len = this->_m.regs[11];
        uint8_t val = this->_m.regs[12];
        fault = guestAddrLenOOB(addr, len);

        if (fault) {
          retval = -1;
        } else {
          void* base = this->_ram + addr;
          memset(base, val, len);
        }
      }
      break;

    // TODO: more memory/str functions

    case 0x10: // delay(ms)
      {
        uint32_t ms = this->_m.regs[10];
        this->_blockedState = BlockedState {
          reason: BLOCKED_DELAY_TILL,
          delayTill: millis() + ms,
        };
        this->_state = STATE_BLOCKED;
      }
      break;

    // start of wifi section
    case 0x100: // wifi_mode_t wifi::getMode()
      retval = WiFi.getMode();
      break;
    case 0x101: // bool wifi::setMode(wifi_mode_t)
      retval = WiFi.mode((wifi_mode_t) this->_m.regs[10]);
      break;
    case 0x102: // int32_t wifi::getChannel()
      retval = WiFi.channel();
      break;
    case 0x103: // bool wifi::isConnected()
      retval = WiFi.isConnected();
      break;
    case 0x104: // bool wifi::begin()
      {
        uint32_t ssid_addr = this->_m.regs[10];
        uint32_t ssid_len = this->_m.regs[11];
        uint32_t pwd_addr = this->_m.regs[12];
        uint32_t pwd_len = this->_m.regs[13];
        uint8_t channel = this->_m.regs[14];

        uint8_t ssidBuff[68];
        uint8_t pwdBuff[33];
        memset(&ssidBuff, 0, 68);
        memset(&pwdBuff, 0, 33);

        fault |= guestAddrLenOOB(ssid_addr, ssid_len);
        fault |= guestAddrLenOOB(pwd_addr, pwd_len);
        fault |= ssid_len > 67;
        fault |= pwd_len > 32;
        if (fault) {
          retval = -1;
        } else {
          WiFi.setSleep(WIFI_PS_NONE); // TODO: MAKE OWN SYSCALL

          strncpy((char*) &ssidBuff, (const char*) (this->_ram+ssid_addr), ssid_len);
          strncpy((char*) &pwdBuff, (const char*) (this->_ram+pwd_addr), pwd_len);
          retval = WiFi.begin((const char*) &ssidBuff, (const char*) &pwdBuff, channel, NULL, true);
        }
      }
      break;
    case 0x105: // bool wifi::ipv4()
      retval = WiFi.localIP();
      break;

    // start of file section
    case 0x130: // close(fd)
      {
        uint32_t fd = this->_m.regs[10];
        if (fd >= MAX_FDs) {
          fault = 1;
        } else {
          switch (this->_files[fd].type) {
            case FILE_UNALLOCATED:
              fault = 2;
              break;
            case FILE_CLIENT:
              this->_files[fd].data.client->stop();
              delete this->_files[fd].data.client;
              this->_files[fd].type = FILE_UNALLOCATED;
              retval = 0;
              break;
            case FILE_SERVER:
              delete this->_files[fd].data.server;
              this->_files[fd].type = FILE_UNALLOCATED;
              retval = 0;
              break;
          }
        }
      }
      break;
    case 0x131: // uint32_t write(fd, data_ptr, data_len)
      {
        uint32_t fd = this->_m.regs[10];
        uint32_t data_addr = this->_m.regs[11];
        uint32_t data_len = this->_m.regs[12];

        fault |= fd >= MAX_FDs;
        fault |= guestAddrLenOOB(data_addr, data_len);
        if (!fault) {
          switch (this->_files[fd].type) {
            case FILE_SERVER:
            case FILE_UNALLOCATED:
              fault = 2;
              break;
            case FILE_CLIENT:
              retval = this->_files[fd].data.client->write((const char*) (this->_ram+data_addr), data_len);
              break;
          }
        }
      }
      break;

    // start of network section
    case 0x140: // fd listen(port)
      {
        uint32_t fd = this->findFreeFd();
        if (fd == -1) {
          fault = 1;
        } else {
          this->_files[fd].data.server = new WiFiServer(this->_m.regs[10]);
          this->_files[fd].data.server->begin();
          this->_files[fd].type = FILE_SERVER;
          retval = fd;
        }
      }
      break;
    case 0x141: // fd accept(fd)
      {
        uint32_t lfd = this->_m.regs[10];
        uint32_t cfd = this->findFreeFd();
        if (cfd == -1 || lfd >= MAX_FDs || this->_files[lfd].type != FILE_SERVER) {
          fault = 1;
        } else {
          WiFiClient c = this->_files[lfd].data.server->available();
          if (c) {
            this->_files[cfd].data.client = new WiFiClient(c);
            this->_files[cfd].type = FILE_CLIENT;
            retval = cfd;
          } else {
            retval = -1;
          }
        }
      }
      break;


    default:
      fault = 2;
      retval = -1;
      break;
  }

  if (!fault) {
    this->_m.regs[10] = retval;
    this->_m.regs[17] = 0;
    // Serial.printf("success with return: %x\n", retval);
  } else {
    this->_m.regs[17] = retval; // error code in syscall number
    // Serial.printf("fault with code: %x\n", fault);
  }

  this->_m.pc = this->_m.mepc + 4;
  this->_m.mtval = 0;
  this->_m.mcause = 0;
  this->_m.extraflags &= ~3; // switch back to user mode
}