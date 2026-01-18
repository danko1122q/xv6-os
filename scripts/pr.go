package main

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"time"
)

const (
	linesPerPage = 50
)

// main prints formatted pages from stdin with headers and page numbers.
// It removes //DOC comments and formats output in 50-line pages with
// a timestamp, header, and optional sheet numbers.
//
// Usage: pr <header>
func main() {
	// Require at least one argument (the header)
	if len(os.Args) < 2 {
		os.Exit(0)
	}

	header := os.Args[len(os.Args)-1]
	now := time.Now().Format("Jan _2 15:04 2006")

	// Read all lines from stdin, removing //DOC comments
	lines := readAndProcessLines()

	// Print lines in paginated format
	printPages(lines, now, header)
}

// readAndProcessLines reads all lines from stdin and removes //DOC comments.
func readAndProcessLines() []string {
	var lines []string
	scanner := bufio.NewScanner(os.Stdin)
	
	for scanner.Scan() {
		line := scanner.Text()
		
		// Remove //DOC comments if present
		if idx := strings.Index(line, "//DOC"); idx != -1 {
			line = line[:idx]
		}
		
		lines = append(lines, line)
	}
	
	return lines
}

// printPages outputs lines in paginated format with headers and footers.
func printPages(lines []string, timestamp, header string) {
	page := 0
	
	for i := 0; i < len(lines); i += linesPerPage {
		page++
		
		// Print page header
		fmt.Printf("\n\n%s  %s  Page %d\n\n\n", timestamp, header, page)

		// Print lines for this page (up to linesPerPage)
		endLine := i + linesPerPage
		if endLine > len(lines) {
			endLine = len(lines)
		}
		
		var j int
		for j = i; j < endLine; j++ {
			fmt.Println(lines[j])
		}
		
		// Fill remaining lines with blank lines to maintain consistent page height
		for ; j < i+linesPerPage; j++ {
			fmt.Println()
		}

		// Extract sheet number from first line of page if present
		sheet := extractSheetNumber(lines, i)
		
		// Print page footer
		fmt.Printf("\n\n%s\n\n\n", sheet)
	}
}

// extractSheetNumber attempts to extract a sheet number from the first line
// of a page. It looks for a 4-character prefix before the first space.
// Returns "Sheet XX" if found, empty string otherwise.
func extractSheetNumber(lines []string, pageStart int) string {
	if pageStart >= len(lines) {
		return ""
	}

	firstLine := lines[pageStart]
	if len(firstLine) < 4 {
		return ""
	}

	// Check if line contains a space
	if !strings.Contains(firstLine, " ") {
		return ""
	}

	// Extract the first token
	parts := strings.Split(firstLine, " ")
	if len(parts) > 0 && len(parts[0]) == 4 {
		// Extract first two characters as sheet number
		return "Sheet " + parts[0][:2]
	}

	return ""
}