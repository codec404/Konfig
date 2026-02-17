package commands

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/codec404/Konfig/pkg/apiclient"
	"github.com/spf13/cobra"
	"gopkg.in/yaml.v3"
)

func NewGetCommand() *cobra.Command {
	var (
		configID string
		output   string
		server   string
	)

	cmd := &cobra.Command{
		Use:   "get [config-id]",
		Short: "Get configuration by ID",
		Long: `Retrieve a configuration by its ID.

Examples:
  konfig get test-service-v1
  konfig get test-service-v5 -o json
  konfig get test-service-v3 -o yaml > config.yaml`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configID = args[0]

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

			// Get config
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()

			resp, err := client.GetConfig(ctx, configID)
			if err != nil {
				return fmt.Errorf("get failed: %w", err)
			}

			if !resp.Success {
				return fmt.Errorf("get failed: %s", resp.Message)
			}

			config := resp.Config

			// Output based on format
			switch output {
			case "json":
				// Pretty print JSON
				data := map[string]interface{}{
					"config_id":    config.ConfigId,
					"service_name": config.ServiceName,
					"version":      config.Version,
					"format":       config.Format,
					"content":      config.Content,
					"created_at":   config.CreatedAt,
					"created_by":   config.CreatedBy,
				}
				jsonBytes, _ := json.MarshalIndent(data, "", "  ")
				fmt.Println(string(jsonBytes))

			case "yaml":
				data := map[string]interface{}{
					"config_id":    config.ConfigId,
					"service_name": config.ServiceName,
					"version":      config.Version,
					"format":       config.Format,
					"content":      config.Content,
					"created_at":   config.CreatedAt,
					"created_by":   config.CreatedBy,
				}
				yamlBytes, _ := yaml.Marshal(data)
				fmt.Print(string(yamlBytes))

			case "content":
				// Just print the content
				fmt.Println(config.Content)

			default:
				// Table format
				fmt.Println("Configuration Details")
				fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
				fmt.Printf("Config ID:     %s\n", config.ConfigId)
				fmt.Printf("Service:       %s\n", config.ServiceName)
				fmt.Printf("Version:       %d\n", config.Version)
				fmt.Printf("Format:        %s\n", config.Format)
				fmt.Printf("Created By:    %s\n", config.CreatedBy)
				fmt.Printf("Created At:    %s\n", time.Unix(config.CreatedAt, 0).Format(time.RFC3339))
				fmt.Println()
				fmt.Println("Content:")
				fmt.Println("─────────────────────────────────────────────────")
				fmt.Println(config.Content)
			}

			return nil
		},
	}

	cmd.Flags().StringVarP(&output, "output", "o", "table", "Output format (table|json|yaml|content)")
	cmd.Flags().StringVar(&server, "server", "", "API server address")

	return cmd
}