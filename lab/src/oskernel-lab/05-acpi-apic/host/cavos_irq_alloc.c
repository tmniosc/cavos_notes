/* host/cavos_irq_alloc.c — chép nguyên logic irqPerCoreAllocate của cavOS cpu/apic.c (2ba0edb) để chạy trên host.
 * Chạy: gcc -O0 -w -o /tmp/a host/cavos_irq_alloc.c && /tmp/a  (không thuộc kernel, không nằm trong SRCS) */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#define MAX_IRQ 256
static uint8_t *irqPerCpu; static uint8_t irqGenericArray[MAX_IRQ]; static uint32_t lapicGenericArray[MAX_IRQ];
static int irqLast = 0; static int NCPU_LOOP = 1; static uint32_t lapic_ids[4] = {0,1,2,3};
uint8_t irqPerCoreAllocate(uint8_t gsi, uint32_t *lapicId) {
  for (int i = 0; i < irqLast; i++) if (irqGenericArray[i] == gsi) { *lapicId = lapicGenericArray[i]; return 32 + i; }
  while (irqLast == 0xff) irqLast++;
  int irqGenericIndex = irqLast;
  if (irqGenericIndex > (MAX_IRQ - 1)) { printf("overflow\n"); exit(1); }
  int min = MAX_IRQ; size_t minIndex = 0;
  for (size_t i = 0; i < NCPU_LOOP; i++) {   /* cavOS: i < 1 // todo: bootloader.smp->cpu_count */
    if (irqPerCpu[i] < min) { irqPerCpu[i] = min; minIndex = i; }
  }
  irqGenericArray[irqGenericIndex] = gsi; lapicGenericArray[irqGenericIndex] = lapic_ids[minIndex];
  irqPerCpu[minIndex]++; irqLast++; *lapicId = lapic_ids[minIndex];
  return irqGenericIndex + 32;
}
int main(void) {
  for (int pass = 0; pass < 2; pass++) {
    NCPU_LOOP = pass ? 4 : 1; irqLast = 0; irqPerCpu = calloc(4, 1);
    printf("loop over %d CPU(s):\n", NCPU_LOOP);
    for (int g = 0; g < 6; g++) { uint32_t l; uint8_t v = irqPerCoreAllocate(16 + g, &l);
      printf("  gsi %d -> vector 0x%02x lapic %u   irqPerCpu = {%u,%u,%u,%u}\n", 16 + g, v, l, irqPerCpu[0], irqPerCpu[1], irqPerCpu[2], irqPerCpu[3]); }
  }
  /* vector 0xff: chỉ số 223 */
  NCPU_LOOP = 1; irqLast = 0; irqPerCpu = calloc(4, 1); uint32_t l; uint8_t v = 0; int idx;
  for (idx = 0; idx < 224; idx++) v = irqPerCoreAllocate((uint8_t)(idx + 1), &l);
  printf("224th distinct GSI -> vector 0x%02x (= APIC spurious vector 0xff)\n", v);
  return 0;
}
