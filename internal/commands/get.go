package commands

import (
	"fmt"

	"github.com/spf13/cobra"
)

func NewGetCommand() *cobra.Command {
	var (
		version int64
		output  string
	)

	cmd := &cobra.Command{
		Use:   "get [service-name]",
		Short: "Get configuration for a service",
		Long: `Retrieve the current or specific version of a service configuration.

Examples:
  configctl get my-service
  configctl get my-service --version 5
  configctl get my-service -o json
  configctl get my-service -o yaml > config.yaml`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			serviceName := args[0]

			fmt.Printf("📥 Fetching configuration for: %s\n", serviceName)
			if version > 0 {
				fmt.Printf("   Version: %d\n", version)
			} else {
				fmt.Printf("   Version: latest\n")
			}
			fmt.Println()

			// TODO: Implement actual API call

			// Mock response
			config := map[string]interface{}{
				"config_id":    "cfg-" + serviceName + "-v1",
				"service_name": serviceName,
				"version":      1,
				"format":       "json",
				"content": map[string]interface{}{
					"max_connections": 100,
					"timeout_ms":      5000,
					"log_level":       "info",
				},
			}

			switch output {
			case "json":
				fmt.Println(`{
  "config_id": "cfg-my-service-v1",
  "service_name": "my-service",
  "version": 1,
  "format": "json",
  "content": {
    "max_connections": 100,
    "timeout_ms": 5000,
    "log_level": "info"
  }
}`)
			case "yaml":
				fmt.Println(`config_id: cfg-my-service-v1
service_name: my-service
version: 1
format: json
content:
  max_connections: 100
  timeout_ms: 5000
  log_level: info`)
			default:
				// Table format
				fmt.Println("Configuration Details:")
				fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
				fmt.Printf("Config ID:     %v\n", config["config_id"])
				fmt.Printf("Service:       %v\n", config["service_name"])
				fmt.Printf("Version:       %v\n", config["version"])
				fmt.Printf("Format:        %v\n", config["format"])
				fmt.Println()
				fmt.Println("Content:")
				fmt.Println(`{
  "max_connections": 100,
  "timeout_ms": 5000,
  "log_level": "info"
}`)
			}

			return nil
		},
	}

	cmd.Flags().Int64VarP(&version, "version", "V", 0, "Specific version (default: latest)")
	cmd.Flags().StringVarP(&output, "output", "o", "table", "Output format (table|json|yaml)")

	return cmd
}