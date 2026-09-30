// Site map for noir.js: page titles and paths relative to the site root.
// Regenerate with obsidian_to_noir.py, or edit by hand when adding a page.
window.SITE = {
 "title": "Học cavOS",
 "pages": [
  {
   "t": "Lý thuyết",
   "c": [
    {
     "t": "Steps",
     "c": [
      {
       "t": "Boot Flow (_start)",
       "h": "ly_thuyet/steps/boot_flow_start.html"
      },
      {
       "t": "Step 00 - Boot & Limine",
       "h": "ly_thuyet/steps/step_00_boot_limine.html"
      },
      {
       "t": "Step 01 - Serial UART",
       "h": "ly_thuyet/steps/step_01_serial_uart.html"
      },
      {
       "t": "Step 02 - Bootloader Parser",
       "h": "ly_thuyet/steps/step_02_bootloader_parser.html"
      },
      {
       "t": "Step 03 - Framebuffer & Console",
       "h": "ly_thuyet/steps/step_03_framebuffer_console.html"
      },
      {
       "t": "Step 04 - Physical Memory Manager",
       "h": "ly_thuyet/steps/step_04_physical_memory_manager.html"
      },
      {
       "t": "Step 05 - Virtual Memory & Paging",
       "h": "ly_thuyet/steps/step_05_virtual_memory_paging.html"
      },
      {
       "t": "Step 06 - GDT & TSS",
       "h": "ly_thuyet/steps/step_06_gdt_tss.html"
      },
      {
       "t": "Step 07 - ACPI",
       "h": "ly_thuyet/steps/step_07_acpi.html"
      },
      {
       "t": "Step 08 - IDT & Interrupts",
       "h": "ly_thuyet/steps/step_08_idt_interrupts.html"
      },
      {
       "t": "Step 09 - APIC & Timer",
       "h": "ly_thuyet/steps/step_09_apic_timer.html"
      },
      {
       "t": "Step 10 - PS2 Keyboard & Mouse",
       "h": "ly_thuyet/steps/step_10_ps2_keyboard_mouse.html"
      },
      {
       "t": "Step 11 - Multitasking & Scheduler",
       "h": "ly_thuyet/steps/step_11_multitasking_scheduler.html"
      },
      {
       "t": "Step 12 - Networking",
       "h": "ly_thuyet/steps/step_12_networking.html"
      },
      {
       "t": "Step 13 - PCI & NIC",
       "h": "ly_thuyet/steps/step_13_pci_nic.html"
      },
      {
       "t": "Step 14 - AHCI & Filesystems",
       "h": "ly_thuyet/steps/step_14_ahci_filesystems.html"
      },
      {
       "t": "Step 15 - Fast Syscalls",
       "h": "ly_thuyet/steps/step_15_fast_syscalls.html"
      },
      {
       "t": "Step 16 - SSE & FPU",
       "h": "ly_thuyet/steps/step_16_sse_fpu.html"
      },
      {
       "t": "Step 17 - Userspace & ELF Loader",
       "h": "ly_thuyet/steps/step_17_userspace_elf_loader.html"
      }
     ]
    },
    {
     "t": "Khái niệm",
     "c": [
      {
       "t": "Cấu trúc os.img",
       "h": "ly_thuyet/khai_niem/cau_truc_os_img.html"
      },
      {
       "t": "GDT",
       "h": "ly_thuyet/khai_niem/gdt.html"
      },
      {
       "t": "HHDM",
       "h": "ly_thuyet/khai_niem/hhdm.html"
      },
      {
       "t": "Higher-Half Kernel",
       "h": "ly_thuyet/khai_niem/higher_half_kernel.html"
      },
      {
       "t": "KASLR & PIE Kernel",
       "h": "ly_thuyet/khai_niem/kaslr_pie_kernel.html"
      },
      {
       "t": "Limine Protocol",
       "h": "ly_thuyet/khai_niem/limine_protocol.html"
      },
      {
       "t": "Long Mode",
       "h": "ly_thuyet/khai_niem/long_mode.html"
      },
      {
       "t": "Paging",
       "h": "ly_thuyet/khai_niem/paging.html"
      },
      {
       "t": "Request-Response Mechanism",
       "h": "ly_thuyet/khai_niem/request_response_mechanism.html"
      },
      {
       "t": "x86 Segmentation",
       "h": "ly_thuyet/khai_niem/x86_segmentation.html"
      }
     ]
    },
    {
     "t": "Tài liệu gốc",
     "c": [
      {
       "t": "Danh mục tài liệu",
       "h": "ly_thuyet/tai_lieu_goc/danh_muc_tai_lieu.html"
      },
      {
       "t": "Intel SDM",
       "h": "ly_thuyet/tai_lieu_goc/intel_sdm.html"
      },
      {
       "t": "UART 16550",
       "h": "ly_thuyet/tai_lieu_goc/uart_16550.html"
      }
     ]
    }
   ],
   "h": "ly_thuyet/ly_thuyet.html"
  },
  {
   "t": "Thực hành",
   "c": [
    {
     "t": "Lab 0x00 - Hello Serial",
     "h": "thuc_hanh/lab_0x00_hello_serial.html"
    },
    {
     "t": "Lab 0x01 - Bootloader Parser",
     "h": "thuc_hanh/lab_0x01_bootloader_parser.html"
    },
    {
     "t": "Lab 0x02 - Framebuffer",
     "h": "thuc_hanh/lab_0x02_framebuffer.html"
    },
    {
     "t": "Lab 0x03 - PMM & VMM",
     "h": "thuc_hanh/lab_0x03_pmm_vmm.html"
    }
   ],
   "h": "thuc_hanh/thuc_hanh.html"
  },
  {
   "t": "Công cụ và môi trường",
   "c": [
    {
     "t": "Cẩm nang build & debug",
     "h": "cong_cu_va_moi_truong/cam_nang_build_debug.html"
    },
    {
     "t": "Dựng môi trường cavOS",
     "h": "cong_cu_va_moi_truong/dung_moi_truong_cavos.html"
    },
    {
     "t": "Dựng môi trường chung",
     "h": "cong_cu_va_moi_truong/dung_moi_truong_chung.html"
    },
    {
     "t": "Dựng môi trường oskernel-lab",
     "h": "cong_cu_va_moi_truong/dung_moi_truong_oskernel_lab.html"
    }
   ],
   "h": "cong_cu_va_moi_truong/cong_cu_va_moi_truong.html"
  }
 ],
 "home": {
  "t": "Home",
  "h": "index.html"
 }
};
