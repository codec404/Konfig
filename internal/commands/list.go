package commands

import (
	"fmt"

	"github.com/spf13/cobra"
)

func NewListCommand() *cobra.Command {
	var (
		service string
		limit   int
	)

	cmd := &cobra.Command{
		Use:   "list",
		Short: "List configurations",
		Long: `List all configurations or filter by service name.

Examples:
  configctl list
  configctl list --service my-service
  configctl list --limit 10`,
		RunE: func(cmd *cobra.Command, args []string) error {
			fmt.Println("📋 Configuration List")
			fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
			fmt.Println()

			// TODO: Implement actual API call

			// Mock data
			configs := []struct {
				Service     string
				Version     int
				UpdatedAt   string
				Instances   int
				Description string
			}{
				{"api-gateway", 5, "2 hours ago", 10, "Latest production config"},
				{"auth-service", 3, "1 day ago", 5, "Auth configuration"},
				{"payment-service", 12, "30 mins ago", 8, "Payment gateway settings"},
			}

			// Table header
			fmt.Printf("%-20s %-10s %-15s %-12s %s\n", "SERVICE", "VERSION", "UPDATED", "INSTANCES", "DESCRIPTION")
			fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

			for _, cfg := range configs {
				if service != "" && cfg.Service != service {
					continue
				}
				fmt.Printf("%-20s %-10d %-15s %-12d %s\n",
					cfg.Service, cfg.Version, cfg.UpdatedAt, cfg.Instances, cfg.Description)
			}

			fmt.Println()
			fmt.Printf("Total: %d configurations\n", len(configs))

			return nil
		},
	}

	cmd.Flags().StringVarP(&service, "service", "n", "", "Filter by service name")
	cmd.Flags().IntVarP(&limit, "limit", "l", 50, "Maximum number of results")

	return cmd
}