package main

import (
	"fmt"
	"os"

	"github.com/codec404/Konfig/internal/commands"
	"github.com/spf13/cobra"
)

var (
	version = "dev"
	commit  = "none"
	date    = "unknown"
)

func main() {
	rootCmd := &cobra.Command{
		Use:   "configctl",
		Short: "Configuration Management CLI",
		Long: `configctl - A CLI tool for managing distributed configurations

Upload, validate, and manage configurations for your services.
Push configuration changes to thousands of services in seconds.`,
		Version: fmt.Sprintf("%s (commit: %s, built: %s)", version, commit, date),
	}

	// Global flags
	rootCmd.PersistentFlags().StringP("server", "s", "localhost:8080", "API server address")
	rootCmd.PersistentFlags().StringP("output", "o", "table", "Output format (table|json|yaml)")
	rootCmd.PersistentFlags().BoolP("verbose", "v", false, "Verbose output")

	// Add commands
	rootCmd.AddCommand(commands.NewUploadCommand())
	rootCmd.AddCommand(commands.NewGetCommand())
	rootCmd.AddCommand(commands.NewListCommand())
	rootCmd.AddCommand(commands.NewDeleteCommand())
	rootCmd.AddCommand(commands.NewValidateCommand())
	rootCmd.AddCommand(commands.NewRollbackCommand())
	rootCmd.AddCommand(commands.NewStatusCommand())
	rootCmd.AddCommand(commands.NewVersionCommand())

	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}