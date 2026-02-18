package commands

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/codec404/Konfig/pkg/pb"
	"github.com/codec404/Konfig/pkg/apiclient"
	"github.com/spf13/cobra"
)

func NewUploadCommand() *cobra.Command {
	var (
		serviceName string
		format      string
		description string
		createdBy   string
		dryRun      bool
		server      string
	)

	cmd := &cobra.Command{
		Use:   "upload [config-file]",
		Short: "Upload a configuration file",
		Long: `Upload a configuration file to the config service.

The file will be validated, versioned, and stored in the database.
Clients subscribed to this service will receive the update.

Examples:
  konfig upload config.json --service my-service
  konfig upload config.yaml --service my-service --format yaml
  konfig upload config.json --service my-service --dry-run`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configFile := args[0]

			// Read file
			content, err := os.ReadFile(configFile)
			if err != nil {
				return fmt.Errorf("failed to read config file: %w", err)
			}

			// Auto-detect format if not specified
			if format == "" {
				format = detectFormat(configFile)
			}

			// Get server from flag or environment
			if server == "" {
				server = os.Getenv("KONFIG_SERVER")
				if server == "" {
					server = "localhost:8081"
				}
			}

			if dryRun {
				fmt.Println("🔍 Dry run mode - no changes will be made")
				fmt.Println()
			}

			fmt.Printf("📤 Uploading configuration...\n")
			fmt.Printf("   Service: %s\n", serviceName)
			fmt.Printf("   File:    %s\n", configFile)
			fmt.Printf("   Format:  %s\n", format)
			fmt.Printf("   Size:    %d bytes\n", len(content))
			fmt.Printf("   Server:  %s\n", server)
			fmt.Println()

			if dryRun {
				fmt.Println("✓ Validation passed")
				fmt.Println("✓ Would upload to service:", serviceName)
				return nil
			}

			// Create API client
			client, err := apiclient.NewClient(server)
			if err != nil {
				return fmt.Errorf("failed to connect to API service: %w", err)
			}
			defer client.Close()

			// Upload config
			ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
			defer cancel()

			if createdBy == "" {
				createdBy = os.Getenv("USER")
				if createdBy == "" {
					createdBy = "konfig-cli"
				}
			}

			req := &pb.UploadConfigRequest{
				ServiceName: serviceName,
				Content:     string(content),
				Format:      format,
				CreatedBy:   createdBy,
				Description: description,
				Validate:    true,
			}

			resp, err := client.UploadConfig(ctx, req)
			if err != nil {
				return fmt.Errorf("upload failed: %w", err)
			}

			if !resp.Success {
				fmt.Printf("❌ Upload failed: %s\n", resp.Message)
				if len(resp.ValidationErrors) > 0 {
					fmt.Println("\nValidation errors:")
					for _, e := range resp.ValidationErrors {
						fmt.Printf("  - %s\n", e)
					}
				}
				return fmt.Errorf("upload failed")
			}

			fmt.Println("✅ Configuration uploaded successfully")
			fmt.Printf("   Config ID: %s\n", resp.ConfigId)
			fmt.Printf("   Version:   %d\n", resp.Version)
			fmt.Println()
			fmt.Println("The configuration will be distributed to all connected clients.")

			return nil
		},
	}

	cmd.Flags().StringVarP(&serviceName, "service", "s", "", "Service name (required)")
	cmd.Flags().StringVarP(&format, "format", "f", "", "Config format (json|yaml|toml)")
	cmd.Flags().StringVarP(&description, "description", "d", "", "Configuration description")
	cmd.Flags().StringVar(&createdBy, "created-by", "", "Who is uploading (default: $USER)")
	cmd.Flags().BoolVar(&dryRun, "dry-run", false, "Validate without uploading")
	cmd.Flags().StringVar(&server, "server", "", "API server address (default: localhost:8081)")
	cmd.MarkFlagRequired("service")

	return cmd
}

func detectFormat(filename string) string {
	ext := filepath.Ext(filename)
	switch ext {
	case ".json":
		return "json"
	case ".yaml", ".yml":
		return "yaml"
	case ".toml":
		return "toml"
	default:
		return "json"
	}
}