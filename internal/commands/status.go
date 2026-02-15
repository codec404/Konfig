package commands

import (
	"fmt"

	"github.com/spf13/cobra"
)

func NewStatusCommand() *cobra.Command {
	cmd := &cobra.Command{
		Use:   "status [service-name]",
		Short: "Show service configuration status",
		Long: `Display the current status of service instances and their configurations.

Examples:
  configctl status my-service
  configctl status --all`,
		Args: cobra.MaximumNArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			var serviceName string
			if len(args) > 0 {
				serviceName = args[0]
			}

			fmt.Println("📊 Service Configuration Status")
			fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
			fmt.Println()

			if serviceName != "" {
				fmt.Printf("Service: %s\n", serviceName)
				fmt.Printf("Current Version: 5\n")
				fmt.Printf("Active Instances: 10\n")
				fmt.Printf("Last Updated: 2 hours ago\n")
				fmt.Println()

				// Instance status
				fmt.Println("Instance Status:")
				fmt.Printf("%-25s %-10s %-15s %s\n", "INSTANCE", "VERSION", "STATUS", "LAST SEEN")
				fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
				fmt.Printf("%-25s %-10d %-15s %s\n", "instance-001", 5, "✓ Connected", "1 min ago")
				fmt.Printf("%-25s %-10d %-15s %s\n", "instance-002", 5, "✓ Connected", "2 min ago")
				fmt.Printf("%-25s %-10d %-15s %s\n", "instance-003", 4, "⚠ Outdated", "5 min ago")
			} else {
				// Show all services
				fmt.Printf("%-20s %-10s %-12s %-15s\n", "SERVICE", "VERSION", "INSTANCES", "STATUS")
				fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
				fmt.Printf("%-20s %-10d %-12d %s\n", "api-gateway", 5, 10, "✓ Healthy")
				fmt.Printf("%-20s %-10d %-12d %s\n", "auth-service", 3, 5, "✓ Healthy")
				fmt.Printf("%-20s %-10d %-12d %s\n", "payment-service", 12, 8, "⚠ 2 outdated")
			}

			return nil
		},
	}

	return cmd
}