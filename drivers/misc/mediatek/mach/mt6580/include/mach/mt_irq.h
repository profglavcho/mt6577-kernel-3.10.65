#ifndef __MT_IRQ_H
#define __MT_IRQ_H


/*
 * Define hadware registers.
 */

#define GIC_PRIVATE_SIGNALS     (32)
#define NR_GIC_SGI              (16)
#define NR_GIC_PPI              (16)
#define GIC_PPI_OFFSET          (27)
#define MT_NR_PPI               (5)
#define MT_NR_SPI               (224)
#define NR_MT_IRQ_LINE          (GIC_PPI_OFFSET + MT_NR_PPI + MT_NR_SPI)

#define GIC_PPI_GLOBAL_TIMER    (GIC_PPI_OFFSET + 0)
#define GIC_PPI_LEGACY_FIQ      (GIC_PPI_OFFSET + 1)
#define GIC_PPI_PRIVATE_TIMER   (GIC_PPI_OFFSET + 2)
#define GIC_PPI_WATCHDOG_TIMER  (GIC_PPI_OFFSET + 3)
#define GIC_PPI_LEGACY_IRQ      (GIC_PPI_OFFSET + 4)
#define WDT_IRQ_BIT_ID            (189)

#if !defined(__ASSEMBLY__)
#define X_DEFINE_IRQ(__name, __num, __pol, __sens)  __name = __num,
enum 
{
#include "x_define_irq.h"
};
#undef X_DEFINE_IRQ
#endif
/* assign a random number since it won't be used */
#endif  /*  !__IRQ_H__ */

