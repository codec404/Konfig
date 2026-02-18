package commands

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	"github.com/spf13/cobra"
)

func NewDeleteCommand() *cobra.Command {
	var (
		force  bool
		server string
	)

	cmd := &cobra.Command{
		Use:   "delete [config-id]",
		Short: "Delete a configuration",
		Long: `Delete a configuration by ID.

WARNING: This cannot be undone!

Examples:
  konfig delete my-service-v5
  konfig delete my-service-v5 --force`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configID := args[0]

			if !force {
				fmt.Printf("⚠️  Are you sure you want to delete %s? (yes/no): ", configID)
				var confirm string
				fmt.Scanln(&confirm)
				if confirm != "yes" {
					fmt.Println("Cancelled")
					return nil
				}
			}

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

			// Delete
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			resp, err := client.DeleteConfig(ctx, configID)
			if err != nil {
				return fmt.Errorf("delete failed: %w", err)
			}

			if !resp.Success {
				return fmt.Errorf("delete failed: %s", resp.Message)
			}

			fmt.Printf("✅ Configuration deleted: %s\n", configID)

			return nil
		},
	}

	cmd.Flags().BoolVar(&force, "force", false, "Skip confirmation")
	cmd.Flags().StringVar(&server, "server", "", "API server address")

	return cmd
}