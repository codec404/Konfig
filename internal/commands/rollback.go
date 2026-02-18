package commands

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	"github.com/spf13/cobra"
)

func NewRollbackCommand() *cobra.Command {
	var (
		toVersion int64
		server    string
	)

	cmd := &cobra.Command{
		Use:   "rollback [service-name]",
		Short: "Rollback to a previous configuration version",
		Long: `Rollback service configuration to a previous version.

Examples:
  konfig rollback my-service --to-version 4
  konfig rollback my-service --to-version 0  # rollback to previous version`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			serviceName := args[0]

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

			// Rollback
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			fmt.Printf("🔄 Rolling back %s to version %d...\n\n", serviceName, toVersion)

			resp, err := client.Rollback(ctx, serviceName, toVersion)
			if err != nil {
				return fmt.Errorf("rollback failed: %w", err)
			}

			if !resp.Success {
				return fmt.Errorf("rollback failed: %s", resp.Message)
			}

			fmt.Println("✅ Rollback successful")
			fmt.Printf("   New Config ID: %s\n", resp.ConfigId)
			fmt.Println()
			fmt.Println(resp.Message)
			fmt.Println()
			fmt.Println("The rollback config will be distributed to all connected clients.")

			return nil
		},
	}

	cmd.Flags().Int64Var(&toVersion, "to-version", 0, "Target version (0 = previous version)")
	cmd.Flags().StringVar(&server, "server", "", "API server address")
	cmd.MarkFlagRequired("to-version")

	return cmd
}