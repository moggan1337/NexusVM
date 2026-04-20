fn main() {
    // Compile inline assembly for VMX operations
    let arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    if arch == "x86_64" {
        cc::Build::new()
            .file("src/vmx/asm.S")
            .compile("vmx_asm");
    }
}
