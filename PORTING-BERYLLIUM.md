# Port to Poco F1 (beryllium, SDM845) — plan

Source: aTechGeekDev/ios-apollo (Mi 10T, SM8250). Target device confirmed over fastboot:
`product: beryllium`, `variant: SDM UFS`, bootloader unlocked, no A/B slots (serial kept out of git).

Rules (from AGENTS.md): nothing is flashed except the iOS root in `userdata`; every boot is a
temporary `fastboot boot`; back up partitions first; confirm each destructive step with the user.

## Milestones

| # | Goal | Needs | Done when |
|---|------|-------|-----------|
| 0 | Backups + device facts | Recovery image with adb for beryllium (OrangeFox/TWRP, booted temporarily), live DTB | `persist`, `modemst1/2`, `fsg`, `fsc`, `boot`, `dtbo`, `vbmeta` saved, SHA256 verified |
| 1 | Shim boots, screen colour | Shim adapted: framebuffer address/size/stride, RAM map, GICv3 (GICD 0x17a00000, GICR 0x17a60000 on SDM845), PSCI | Solid colour / text on the panel from `fastboot boot` |
| 2 | UFS read | `shim/ufs.c` for SDM845 UFS 2.1 (UFS HC 0x1d84000, PHY differs from SM8250) | Shim reads the payload/root from `userdata` |
| 3 | XNU reaches fbcon | `mkadt.py` for SDM845 (cpus, timer, GIC, memory-map), xnu patches 0004/0016-0018 machine config, 8 cores Kryo 385 (4+4) | Kernel log on screen, no panic |
| 4 | iOS root, launchd | `build-ios-root.sh` (iPhone X IPSW), display scaling for 1080x2246 | SpringBoard drawing in software |
| 5 | Touch | Panel variant (Tianma NT36672A vs EBBG FT8719) decides the driver | Taps work |
| 6 | Buttons, backlight, CPU freq | PMI8998/PM8998 GPIO keys, WLED, cpufreq for SDM845 (0095/0098/0153 rewritten) | |
| 7 | GPU | Adreno 630: a630_sqe.fw, a630_gmu.bin, zap shader; Mesa freedreno a6xx | GPU-drawn UI |
| 8 | Wi-Fi / BT | WCN3990 (ath10k snoc) instead of QCA6390 (ath11k+PCIe/MHI): new layer, `wifi/` rewritten | |
| 9 | Audio / haptics / mic | WCD9341 + TFA9894 (no CS35L41/AW8697) | |

## Apollo-specific code to replace (found by grep)

- `tools/mkadt.py` — all SoC addresses, cpus, interrupts
- `shim/shim.c`, `shim/ufs.c` — memory map, UFS controller
- `xnu/patches/0083-0094` touch, `0095/0098/0153` cpufreq+thermal (TSENS, EPSS -> SDM845 uses OSM/CPUFREQ-HW differently),
  `0100-0112` GPU/GMU/ZAP, `0126-0135` Wi-Fi (PCIe MHI), `0136-0145` BT, `0139` audio, `0147` haptics, `0148/0153` DDR (RPMh)
- `tools/pull-*-fw.sh`, `tools/nvt-fw-find.py`, `tools/gmu-fw-info.py`, `tools/acdb-mic.py`
- `userland/mesa/sweet-drm`, `userland/audio`, `userland/bt-inject`

## References for beryllium
- Mainline Linux: `arch/arm64/boot/dts/qcom/sdm845-xiaomi-beryllium-{tianma,ebbg}.dts`, `sdm845.dtsi`
- Xiaomi kernel source: MiCode/Xiaomi_Kernel_OpenSource, branch `beryllium-p-oss`
- postmarketOS device-xiaomi-beryllium

## Milestone 0 results (2026-10-30)
- Recovery: OrangeFox R11.1 stable (official, md5 895d5f1e73b59c5d63bed942752dfaf8) -> `recovery/recovery.img`, booted with `fastboot boot` (kernel 4.9.239). If `adb devices` shows nothing, `adb kill-server && adb start-server` (the adb server crashed once on a wifi-adb TLS handshake).
- Partition names: `/dev/block/platform/soc/1d84000.ufshc/by-name/*`. busybox `dd` needs `bs=1048576` (not `1M`); `adb exec-out` is not supported by this adbd: dd to /tmp, then `adb pull`.
- Backed up and SHA256-verified on both sides: persist, modemst1, modemst2, fsg, fsc, boot, dtbo, vbmeta -> `backup/` (+ `dtb/live.dtb`, `dtb/live.dts`). Keep out of git.
- Panel: **Tianma NT36672A, DSI video mode**, 1080x2246@60 (`dsi_tianma_fhd_nt36672a_video_display`). Touch is therefore the Novatek NT36672A path.
- Continuous splash framebuffer: **0x9d400000, 0x2400000 bytes** (live DT `cont_splash_region`).
- UFS controller `1d84000.ufshc`. RAM: System RAM from 0x80000000; `ramoops@b0000000`, `ramdump_fb_region@af000000`.
- Partition table has `_a/_b` names for firmware (abl, xbl, tz ...) but fastboot reports no slots for boot/system.

## Milestones 1-2 results (2026-10-30)
- **M1 done:** shim runs on beryllium and draws on the cont-splash framebuffer 0x9d400000 (1080x2246, stride 4320; photo-verified, text under the notch is clipped). Needed: kernel as **gzip + appended DTB** (`Image.gz-dtb`, `dtb/stock_appended.dtb`, msm-id 0x141/0x20001, board-id 0x45); a raw `Image` is accepted ("OKAY") and then refused (`00000050`).
- ABL quirk: `fastboot boot` works **once per bootloader session**; after any attempt do `fastboot reboot bootloader` first.
- Boot header v0 (tools/mkbootimg_v0.py): base 0, kernel_addr 0x8000, ramdisk 0x1000000, tags 0x100, os_version 0x2004063c. ABL loads the ramdisk at 0x855FE000 (INITRD lo, from the loader line).
- No serial/log channel survives the shim's reset (ramoops via PSCI reset -> bootloader is wiped); the screen is the channel. `shim/crumb.c` writes a pstore-format log to 0xb0180000 (works across Linux warm reboots only).
- **M2 done (read-only):** shim's UFS driver works unchanged on SDM845 UFS 2.1 (HC 0x1d84000, ver 0x210): TUR/CAP/READ rc=0, LUN0 last LBA 0xDC07FF, block 4096, MBR 0xAA55 + GPT found.
- GPT of LUN0 (sda), LBA in 4K blocks, sizes MiB: persist 0x4000/64, persistbak 0x8000/64, logdump 0xC000/64, minidump 0x10000/128, cust 0x18000/832 (ext4), recovery 0x4C000/64, cache 0x50000/256 (ext4), **esp 0x60000/334, win 0x74E00/47172 (0xBA44), userdata 0xC19200/6773 (ext4)**.
  -> this phone has a Windows-on-ARM layout (esp + win). userdata is only ~6.6 GB; `win` is 46 GB.
  **Open question for the user: where the iOS root goes (userdata 6.6 GB vs win 46 GB). Nothing is written until confirmed.**

## Decision (2026-10-30, user)
- Allowed to erase: **win, esp, userdata, cache, cust** (user data only). iOS root goes into **win** (46 GB).
- Never touched: xbl, abl, tz, hyp, aop, keymaster, cmnlib, devcfg, modem*, fsg, fsc, persist*, dsp, bluetooth, etc.
- Timing: only right before the install, when kernel + iOS root are built and a no-write boot is verified.
  Before erasing: back up `win` (46 GB) and the GPT of every LUN; re-confirm the exact command with the user once.

## Status / next (2026-10-30 late)
- `tools/mkadt.py` adapted for the basics (FB 0x9d400000 1080x2246, vtimer INTID 19, 4+4 cores 1766/2803 MHz, RAM base 0xb0400000 / highmem 3 GiB) and builds an ADT (`--highmem`). Untested on hardware. Needs `research/ipsw-x/d22ap.adt` (iPhone 8 DT from the iOS 16.7.16 IPSW via tools/remotezip.py + tools/adt.py).
- Still apollo-only in mkadt/xnu: touch (NT36675 SPI -> NT36672A), cpufreq (EPSS -> SDM845 OSM/CPUFREQ-HW), TSENS, RPMh DDR, GPU a650 -> a630, wifi/bt, audio, haptics.
- NEXT: (1) create a private GitHub repo from this tree and run `xnu.yml` (kernel build, macOS runners) -- needs user's OK to push/spend Actions minutes; (2) fetch d22ap.adt; (3) boot XNU to fbcon (M3).
