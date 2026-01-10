// © 2025 Unit Circle Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// See: https://interrupt.memfault.com/blog/cortex-m-fault-debug
// https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortex-m4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "log.h"
#include "fault.h"

#include "nrf.h"

#define SCB_UFSR  (SCB->CFSR & SCB_CFSR_USGFAULTSR_Msk)
#define SCB_BFSR  (SCB->CFSR & SCB_CFSR_BUSFAULTSR_Msk)
#define SCB_MMFSR (SCB->CFSR & SCB_CFSR_MEMFAULTSR_Msk)

#define EXC_RETURN_PREFIX  (0xff000000)
#define EXC_RETURN_NOFPU   (0x00000010)
#define EXC_RETURN_THREAD  (0x00000008)
#define EXC_RETURN_PSP     (0x00000004)

typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  // Only if floating point HW enabled
  uint32_t s[16];
  uint32_t fpsrc;
  uint32_t undefined;
#endif
} stack_t;

static void memmanage_decode(bool from_hardfault) {
  LOG_PANIC("MemManage");
  if (SCB->CFSR & SCB_CFSR_MSTKERR_Msk) {
    LOG_PANIC("  Stacking access error");
  }
  else if (SCB->CFSR & SCB_CFSR_MUNSTKERR_Msk) {
    LOG_PANIC("  Unstacking access error");
  }
  else if (SCB->CFSR & SCB_CFSR_DACCVIOL_Msk) {
    uint32_t mmfar = SCB->MMFAR;
    __DMB();
    if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) {
      LOG_PANIC("  Data access error: 0x%08lx", mmfar);
      if (from_hardfault) SCB->CFSR &= ~SCB_CFSR_MMARVALID_Msk;
    }
    else {
      LOG_PANIC("  Data access error");
    }
  }
  else if (SCB->CFSR & SCB_CFSR_IACCVIOL_Msk) {
    LOG_PANIC("  Instruction access violation");
  }
  else if (SCB->CFSR & SCB_CFSR_MLSPERR_Msk) {
    LOG_PANIC("  Floating-point lazy state preservation access error");
  }
  SCB->CFSR |= SCB_CFSR_MEMFAULTSR_Msk;
}

static void busfault_decode(bool from_hardfault) {
  LOG_PANIC("BusFault");
  if (SCB->CFSR & SCB_CFSR_STKERR_Msk) {
    LOG_PANIC("  Stacking bus error");
  }
  else if (SCB->CFSR & SCB_CFSR_UNSTKERR_Msk) {
    LOG_PANIC("  Unstacking bus error");
  }
  else if (SCB->CFSR & SCB_CFSR_PRECISERR_Msk) {
    uint32_t bfar = SCB->BFAR;
    __DMB();
    if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) {
      LOG_PANIC("  Precise data bus error: 0x%08lx", bfar);
      if (from_hardfault) SCB->CFSR &= ~SCB_CFSR_BFARVALID_Msk;
    }
    else {
      LOG_PANIC("  Precise data bus error");
    }
  }
  else if (SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) {
      LOG_PANIC("  Imprecise data bus error");
    }
  else if (SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) {
    LOG_PANIC("  Imprecise data bus error");
  }
  else if (SCB->CFSR & SCB_CFSR_IBUSERR_Msk) {
    LOG_PANIC("  Instruction bus error");
  }
  else if (SCB->CFSR & SCB_CFSR_LSPERR_Msk) {
    LOG_PANIC("  Floating point lazy state preservation bus error");
  }
  SCB->CFSR |= SCB_CFSR_BUSFAULTSR_Msk;
}

static void usagefault_decode(void) {
  LOG_PANIC("UsageFault");
  if (SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk) LOG_PANIC("  Divide by 0");
  if (SCB->CFSR & SCB_CFSR_UNALIGNED_Msk) LOG_PANIC("  Unaligned access");
  if (SCB->CFSR & SCB_CFSR_NOCP_Msk)      LOG_PANIC("  No coprocessor");
  if (SCB->CFSR & SCB_CFSR_INVPC_Msk)     LOG_PANIC("  Invalid PC");
  if (SCB->CFSR & SCB_CFSR_INVSTATE_Msk)  LOG_PANIC("  Invalid State");
  if (SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk)LOG_PANIC("  Undefined instruction");
  SCB->CFSR |= SCB_CFSR_USGFAULTSR_Msk;
}

static void hardfault_decode(void) {
  LOG_PANIC("HardFault");
  if (SCB->HFSR & SCB_HFSR_VECTTBL_Msk) {
    LOG_PANIC("  Bus fault on vector table read");
  }
  else if (SCB->HFSR & SCB_HFSR_DEBUGEVT_Msk) {
    LOG_PANIC("  Debug event");
  }
  else if (SCB->HFSR & SCB_HFSR_FORCED_Msk) {
    LOG_PANIC("  Fault escalation...");
    if (SCB_MMFSR) {
      memmanage_decode(true);
    }
    else if (SCB_BFSR) {
      busfault_decode(true);
    }
    else if (SCB_UFSR) {
      usagefault_decode();
    }
  }
}

void fault_handler(const stack_t *stack, uint32_t lr) {
  uint32_t cfsr = SCB->CFSR;

  log_panic();

  if (stack) {
    uint32_t fault = SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk;
    switch (fault) {
      case 3:  hardfault_decode(); break;
      case 4:  memmanage_decode(false); break;
      case 5:  busfault_decode(false); break;
      case 6:  usagefault_decode(); break;
      default: LOG_PANIC("  unhandled exception type: %lu", fault); break;
    }

    // Dump the "known stack"
    LOG_PANIC("  r0:   %08lx r1:   %08lx r2:   %08lx r3:   %08lx",
        stack->r0, stack->r1, stack->r2, stack->r3);
    LOG_PANIC("  r12:  %08lx lr:   %08lx pc:   %08lx psr:  %08lx",
        stack->r12, stack->lr, stack->pc, stack->psr);
    LOG_PANIC("  cfsr: %08lx hfsr: %08lx dfsr: %08lx afsr: %08lx",
        cfsr, SCB->HFSR, SCB->DFSR, SCB->AFSR);
    LOG_PANIC("lr_cur: %08lx sp:   %08lx", lr, (uint32_t) stack);

    // Skip what has already been printed
    uint32_t *stack_ = ((uint32_t*) stack) + 8;
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    if ((lr & EXC_RETURN_NOFPU) == 0) {
      __asm volatile("vsub.f32 s16,s16,s16": : ); // Force stacking of FPU regs
      __DSB();
      __ISB();
      LOG_PANIC("  s0:   %08lx s1:   %08lx s2:   %08lx s3:   %08lx",
          stack->s[0], stack->s[1], stack->s[2], stack->s[3]);
      LOG_PANIC("  s4:   %08lx s5:   %08lx s6:   %08lx s7:   %08lx",
          stack->s[4], stack->s[5], stack->s[6], stack->s[7]);
      LOG_PANIC("  s8:   %08lx s9:   %08lx s10:  %08lx s11:  %08lx",
          stack->s[8], stack->s[9], stack->s[10], stack->s[11]);
      LOG_PANIC("  s12:  %08lx s13:  %08lx s14:  %08lx s15:  %08lx",
          stack->s[12], stack->s[13], stack->s[14], stack->s[15]);
      LOG_PANIC("fpfrc:  %08lx", stack->fpsrc);
      stack_ += 18; // Skip printed FPU regs
    }
#endif

    // Dump some of the stack in the hope that we can unwind after the fact
    extern uint32_t __StackTop[];
    uint32_t n = __StackTop - stack_;
    if (n > 32) n = 32;
    while (n > 0) {
      size_t nn = n > 4 ? 4 : n;
      LOG_MEM_PANIC("stack:", stack_, nn * sizeof(uint32_t));
      stack_ += nn;
      n -= nn;
    }
  }
  else {
    LOG_PANIC("Invalid EXC_RETURN value %08lx", lr);
  }

  fault_force_reset();
}


void fault_force_reset(void) {
  // If debugger connected issue breakpoint
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0) __BKPT(0);
  NVIC_SystemReset();
}

#define HANDLER(x) \
__attribute__((naked, noreturn)) void x(void) { \
  /* Setup r0 with start point based on lr bit 2 (EXC_RETURN_PSP) */ \
  /* Then call C version to extract stack and various fault registers */ \
  __asm volatile ( \
    " tst lr, #4      \n" \
    " ite eq          \n" \
    " mrseq r0, msp   \n" \
    " mrsne r0, psp   \n" \
    " mov r1, lr   \n" \
    " b fault_handler \n" \
  ); \
}

HANDLER(HardFault_Handler)
HANDLER(MemManage_Handler)
HANDLER(BusFault_Handler)
HANDLER(UsageFault_Handler)

void fault_init(void) {
  SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk
             |  SCB_SHCSR_BUSFAULTENA_Msk
             |  SCB_SHCSR_MEMFAULTENA_Msk;
}
