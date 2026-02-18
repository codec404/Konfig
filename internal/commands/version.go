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
			fmt.Println("konfig version 1.0.0")
			fmt.Println("Built with Go for dynamic configuration management")
		},
	}
}