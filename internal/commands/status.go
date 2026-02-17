package commands

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	"github.com/spf13/cobra"
)

func NewStatusCommand() *cobra.Command {
	var server string

	cmd := &cobra.Command{
		Use:   "status [config-id]",
		Short: "Show rollout status for a configuration",
		Long: `Display the rollout status of a configuration.

Examples:
  konfig status my-service-v5`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configID := args[0]

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

			// Get status
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			resp, err := client.GetRolloutStatus(ctx, configID)
			if err != nil {
				return fmt.Errorf("status query failed: %w", err)
			}

			if !resp.Success {
				return fmt.Errorf("status query failed")
			}

			state := resp.RolloutState

			fmt.Println("📊 Rollout Status")
			fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
			fmt.Printf("Config ID:     %s\n", state.ConfigId)
			fmt.Printf("Strategy:      %s\n", state.Strategy)
			fmt.Printf("Progress:      %d%% / %d%%\n", state.CurrentPercentage, state.TargetPercentage)
			fmt.Printf("Status:        %s\n", state.Status)
			fmt.Printf("Started:       %s\n", time.Unix(state.StartedAt, 0).Format(time.RFC3339))
			if state.CompletedAt > 0 {
				fmt.Printf("Completed:     %s\n", time.Unix(state.CompletedAt, 0).Format(time.RFC3339))
			}
			fmt.Println()

			if len(resp.Instances) > 0 {
				fmt.Println("Instances:")
				fmt.Printf("%-30s %-10s %-15s\n", "INSTANCE ID", "VERSION", "STATUS")
				fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
				for _, inst := range resp.Instances {
					fmt.Printf("%-30s %-10d %-15s\n",
						truncate(inst.InstanceId, 30),
						inst.CurrentConfigVersion,
						inst.Status)
				}
			}

			return nil
		},
	}

	cmd.Flags().StringVar(&server, "server", "", "API server address")

	return cmd
}