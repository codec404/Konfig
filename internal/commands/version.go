package commands

import (
	"fmt"

	"github.com/spf13/cobra"
)

func NewVersionCommand() *cobra.Command {
	return &cobra.Command{
		Use:   "version",
		Short: "Show version information",
		Run: func(cmd *cobra.Command, args []string) {
			// Version is handled by root command
			cmd.Root().Println(cmd.Root().Version)
		},
	}
}

func NewDeleteCommand() *cobra.Command {
	return &cobra.Command{
		Use:   "delete [service-name] --version [version]",
		Short: "Delete a configuration version",
		Long: `Delete a specific configuration version.

WARNING: This cannot be undone!

Examples:
  configctl delete my-service --version 5
  configctl delete my-service --version 5 --force`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			fmt.Println("🗑️  Delete operation not yet implemented")
			return nil
		},
	}
}

func NewRollbackCommand() *cobra.Command {
	return &cobra.Command{
		Use:   "rollback [service-name] --to-version [version]",
		Short: "Rollback to a previous configuration version",
		Long: `Rollback service configuration to a previous version.

Examples:
  configctl rollback my-service --to-version 4
  configctl rollback my-service --to-version 4 --force`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			fmt.Println("⏮️  Rollback operation not yet implemented")
			return nil
		},
	}
}