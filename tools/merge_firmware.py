Import("env")
import os

def post_build_merged_bin(source, target, env):
    # Récupération des variables d'environnement
    flash_size = env.GetProjectOption("board_upload.flash_size", "4MB")
    
    # Chemins des fichiers
    build_dir = env.subst("$BUILD_DIR")
    firmware = os.path.join(build_dir, "firmware.bin")
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    merged_bin = os.path.join(build_dir, "all-firmware.bin")

    # Localisation de boot_app0.bin dans le framework Arduino
    platform = env.PioPlatform()
    framework_dir = platform.get_package_dir("framework-arduinoespressif32")
    boot_app0 = os.path.join(framework_dir, "tools", "partitions", "boot_app0.bin")

    # Commande esptool pour fusionner
    # On utilise -m esptool pour être sûr d'utiliser celui de PlatformIO
    cmd = [
        "$PYTHONEXE", "-m", "esptool", "--chip", "esp32c3", "merge_bin",
        "-o", merged_bin,
        "--flash_mode", "dio",
        "--flash_size", flash_size,
        "0x0000", bootloader,
        "0x8000", partitions,
        "0xe000", boot_app0,
        "0x10000", firmware
    ]
    
    print(f"\n[POST-BUILD] Generating merged binary for easy flashing: {merged_bin}")
    env.Execute(" ".join(cmd))

# Lancer le script après la création du firmware.bin
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_build_merged_bin)
