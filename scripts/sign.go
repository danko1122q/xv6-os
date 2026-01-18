package main

import (
	"fmt"
	"os"
)

const (
	// maxBootSize is the maximum size for a boot block (510 bytes + 2 byte signature)
	maxBootSize = 510
	// bootBlockSize is the total size including signature
	bootBlockSize = 512
	// bootSignature is the magic number that marks a valid boot sector
	bootSignatureByte1 = 0x55
	bootSignatureByte2 = 0xAA
)

// main adds a boot sector signature (0x55AA) to a boot block file.
// The boot block must be <= 510 bytes; this tool pads it to 512 bytes
// and appends the signature at bytes 510-511.
func main() {
	if len(os.Args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: sign <file>\n")
		os.Exit(1)
	}

	filename := os.Args[1]
	
	// Read the boot block file
	data, err := os.ReadFile(filename)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file %s: %v\n", filename, err)
		os.Exit(1)
	}

	// Validate boot block size
	n := len(data)
	if n > maxBootSize {
		fmt.Fprintf(os.Stderr, "boot block too large: %d bytes (max %d)\n", n, maxBootSize)
		os.Exit(1)
	}

	fmt.Fprintf(os.Stderr, "boot block is %d bytes (max %d)\n", n, maxBootSize)

	// Create a 512-byte buffer initialized with zeros
	buf := make([]byte, bootBlockSize)
	copy(buf, data)

	// Add the boot sector signature 0x55AA at the end
	// This signature is required by BIOS to identify a bootable disk
	buf[510] = bootSignatureByte1
	buf[511] = bootSignatureByte2

	// Write the signed boot block back to the file
	err = os.WriteFile(filename, buf, 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error writing file %s: %v\n", filename, err)
		os.Exit(1)
	}
}