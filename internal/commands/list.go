package commands

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	"github.com/spf13/cobra"
)

func NewListCommand() *cobra.Command {
	var (
		service string
		limit   int32
		offset  int32
		server  string
	)

	cmd := &cobra.Command{
		Use:   "list",
		Short: "List configurations",
		Long: `List all configurations or filter by service name.

Examples:
  konfig list
  konfig list --service my-service
  konfig list --limit 10`,
		RunE: func(cmd *cobra.Command, args []string) error {
			// Get server
			if server == "" {
				server = os.Getenv("KONFIG_SERVER")
				if server == "" {
					server = "localhost:8081"
				}
			}

			// Create client
			client, err := apiclient.NewClient(server)
			if err != nil {
				return fmt.Errorf("failed to connect: %w", err)
			}
			defer client.Close()

			// List configs
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			resp, err := client.ListConfigs(ctx, service, limit, offset)
			if err != nil {
				return fmt.Errorf("list failed: %w", err)
			}

			if !resp.Success {
				return fmt.Errorf("list failed")
			}

			if len(resp.Configs) == 0 {
				fmt.Println("No configurations found")
				return nil
			}

			fmt.Println("📋 Configuration List")
			fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
			fmt.Println()

			// Table header
			fmt.Printf("%-30s %-20s %-8s %-20s %s\n",
				"CONFIG ID", "SERVICE", "VERSION", "CREATED BY", "CREATED AT")
			fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

			for _, cfg := range resp.Configs {
				createdAt := time.Unix(cfg.CreatedAt, 0).Format("2006-01-02 15:04")
				fmt.Printf("%-30s %-20s %-8d %-20s %s\n",
					truncate(cfg.ConfigId, 30),
					truncate(cfg.ServiceName, 20),
					cfg.Version,
					truncate(cfg.CreatedBy, 20),
					createdAt)
			}

			fmt.Println()
			fmt.Printf("Total: %d configurations\n", resp.TotalCount)

			return nil
		},
	}

	cmd.Flags().StringVarP(&service, "service", "s", "", "Filter by service name")
	cmd.Flags().Int32VarP(&limit, "limit", "l", 50, "Maximum number of results")
	cmd.Flags().Int32Var(&offset, "offset", 0, "Pagination offset")
	cmd.Flags().StringVar(&server, "server", "", "API server address")

	return cmd
}

func truncate(s string, max int) string {
	if len(s) <= max {
		return s
	}
	return s[:max-3] + "..."
}