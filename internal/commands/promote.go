package commands

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	pb "github.com/codec404/Konfig/pkg/pb"
	"github.com/spf13/cobra"
)

func NewPromoteCommand() *cobra.Command {
	var server string

	cmd := &cobra.Command{
		Use:   "promote [config-id]",
		Short: "Promote a CANARY rollout to all instances",
		Long: `Promote a CANARY rollout that is IN_PROGRESS to all connected instances.

This pushes the configuration to all remaining instances and marks the rollout
as COMPLETED. Use this after verifying the canary instances look healthy.

Example:
  configctl promote my-service-v5`,
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

			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			// Check current status first
			statusResp, err := client.GetRolloutStatus(ctx, configID)
			if err != nil {
				return fmt.Errorf("failed to get rollout status: %w", err)
			}
			if statusResp.Success && statusResp.RolloutState != nil {
				s := statusResp.RolloutState.Status.String()
				if s != "IN_PROGRESS" {
					return fmt.Errorf("rollout is %s, can only promote IN_PROGRESS rollouts", s)
				}
				if statusResp.RolloutState.Strategy.String() != "CANARY" {
					return fmt.Errorf("rollout strategy is %s, promote is only for CANARY rollouts",
						statusResp.RolloutState.Strategy.String())
				}
			}

			fmt.Printf("Promoting %s to all instances...\n", configID)

			resp, err := client.StartRollout(ctx, &pb.StartRolloutRequest{
				ConfigId:         configID,
				Strategy:         pb.RolloutStrategy_ALL_AT_ONCE,
				TargetPercentage: 100,
			})
			if err != nil {
				return fmt.Errorf("promote failed: %w", err)
			}
			if !resp.Success {
				return fmt.Errorf("promote failed: %s", resp.Message)
			}

			fmt.Println("✓ Promotion started — pushing to all instances")
			fmt.Printf("  Check progress with: configctl status %s\n", configID)
			return nil
		},
	}

	cmd.Flags().StringVar(&server, "server", "", "API server address (default: localhost:8081)")
	return cmd
}
