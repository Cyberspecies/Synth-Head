Import("env")
import os

# Create empty certificate bundle file to satisfy CMake
build_dir = env.subst("$BUILD_DIR")
bundle_path = os.path.join(build_dir, "x509_crt_bundle")
bundle_s_path = os.path.join(build_dir, "x509_crt_bundle.S")

# Create directories if needed
os.makedirs(build_dir, exist_ok=True)

# Create empty bundle file
if not os.path.exists(bundle_path):
    with open(bundle_path, "wb") as f:
        f.write(b"")
    print(f"Created empty certificate bundle: {bundle_path}")

# Create empty .S file
if not os.path.exists(bundle_s_path):
    with open(bundle_s_path, "w") as f:
        f.write("""
.section .rodata.certs_bundle
.global x509_crt_bundle_start
.global x509_crt_bundle_end
.align 4
x509_crt_bundle_start:
x509_crt_bundle_end:
""")
    print(f"Created stub certificate bundle .S: {bundle_s_path}")
