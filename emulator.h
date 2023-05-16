#ifndef Emulator_h
#define Emulator_h

#include <WiFi.h>

enum EmuState { STATE_NORMAL, STATE_BLOCKED, STATE_EXIT_OK, STATE_EXIT_ERR };

enum BlockedReason { BLOCKED_DELAY_TILL };

struct BlockedState
{
  BlockedReason reason;
  uint32_t delayTill;
};

enum FileType { FILE_UNALLOCATED, FILE_SERVER, FILE_CLIENT };

#define MAX_FDs 32

struct FDEntry
{
  FileType type;
  union {
    WiFiServer* server;
    WiFiClient* client;
  } data;
};

// As a note: We quouple-ify these, because in HLSL, we will be operating with
// uint4's.  We are going to uint4 data to/from system RAM.
//
// We're going to try to keep the full processor state to 12 x uint4.
struct MiniRV32IMAState
{
  uint32_t regs[32];

  uint32_t pc;
  uint32_t mstatus;
  uint32_t cyclel;
  uint32_t cycleh;

  uint32_t timerl;
  uint32_t timerh;
  uint32_t timermatchl;
  uint32_t timermatchh;

  uint32_t mscratch;
  uint32_t mtvec;
  uint32_t mie;
  uint32_t mip;

  uint32_t mepc;
  uint32_t mtval;
  uint32_t mcause;

  // Note: only a few bits are used.  (Machine = 3, User = 0)
  // Bits 0..1 = privilege.
  // Bit 2 = WFI (Wait for interrupt)
  // Bit 3+ = Load/Store reservation LSBs.
  uint32_t extraflags;
};

class Emulator {
public:
	Emulator(uint8_t* _ram, uint32_t _ramSize);
  ~Emulator();

  EmuState state();

  // step executes at most numStep instructions or one syscall.
	void step(int numSteps);
  bool noTrap();

private:
	uint8_t* _ram;
	uint32_t _ramSize;
	MiniRV32IMAState _m;
  EmuState _state;
  BlockedState _blockedState;
  FDEntry _files[MAX_FDs];

	// defined in mini-rv32ima.h
	int32_t MiniRV32IMAStep( struct MiniRV32IMAState * state, uint8_t * image, uint32_t elapsedUs, int count );

	void dumpState( struct MiniRV32IMAState * core, uint8_t * ram_image, uint32_t pc );
  uint32_t guestAddrLenOOB(uint32_t baseAddr, uint32_t size);

  void handleSyscall();
  uint32_t findFreeFd();
};

#endif
