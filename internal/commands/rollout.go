package commands

import (
	"context"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	pb "github.com/codec404/Konfig/pkg/pb"
	"github.com/spf13/cobra"
)

func NewRolloutCommand() *cobra.Command {
	var (
		strategy   string
		percentage int32
		server     string
	)

	cmd := &cobra.Command{
		Use:   "rollout [config-id]",
		Short: "Start a rollout for a configuration",
		Long: `Start a rollout to distribute a configuration to service instances.

Strategies:
  ALL_AT_ONCE  Push to all connected instances immediately (default)
  CANARY       Push to ~10% of instances first; stays IN_PROGRESS for review
  PERCENTAGE   Push to a specific percentage of instances

Examples:
  configctl rollout my-service-v2 --strategy ALL_AT_ONCE
  configctl rollout my-service-v2 --strategy CANARY
  configctl rollout my-service-v2 --strategy PERCENTAGE --percentage 50`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configID := args[0]

			if server == "" {
				server = os.Getenv("KONFIG_SERVER")
				if server == "" {
					server = "localhost:8081"
				}
			}

			client, err := apiclient.NewClient(server)
			if err != nil {
				return fmt.Errorf("failed to connect: %w", err)
			}
			defer client.Close()

			// Map strategy string to proto enum
			var rolloutStrategy pb.RolloutStrategy
			switch strings.ToUpper(strategy) {
			case "ALL_AT_ONCE", "":
				rolloutStrategy = pb.RolloutStrategy_ALL_AT_ONCE
			case "CANARY":
				rolloutStrategy = pb.RolloutStrategy_CANARY
			case "PERCENTAGE":
				rolloutStrategy = pb.RolloutStrategy_PERCENTAGE
				if percentage <= 0 || percentage > 100 {
					return fmt.Errorf("--percentage must be between 1 and 100")
				}
			default:
				return fmt.Errorf("unknown strategy %q (valid: ALL_AT_ONCE, CANARY, PERCENTAGE)", strategy)
			}

			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			fmt.Printf("Starting rollout for %s (strategy: %s", configID, strings.ToUpper(strategy))
			if rolloutStrategy == pb.RolloutStrategy_PERCENTAGE {
				fmt.Printf(", target: %d%%", percentage)
			}
			fmt.Println(")")

			resp, err := client.StartRollout(ctx, &pb.StartRolloutRequest{
				ConfigId:         configID,
				Strategy:         rolloutStrategy,
				TargetPercentage: percentage,
			})
			if err != nil {
				return fmt.Errorf("rollout failed: %w", err)
			}

			if !resp.Success {
				return fmt.Errorf("rollout failed: %s", resp.Message)
			}

			fmt.Println("✓ Rollout started")
			fmt.Printf("  Rollout ID: %s\n", resp.RolloutId)
			fmt.Printf("  %s\n", resp.Message)
			fmt.Println()
			fmt.Printf("Check progress with: configctl status %s\n", configID)

			return nil
		},
	}

	cmd.Flags().StringVar(&strategy, "strategy", "ALL_AT_ONCE", "Rollout strategy (ALL_AT_ONCE|CANARY|PERCENTAGE)")
	cmd.Flags().Int32Var(&percentage, "percentage", 100, "Target percentage for PERCENTAGE strategy")
	cmd.Flags().StringVar(&server, "server", "", "API server address (default: localhost:8081)")

	return cmd
}
