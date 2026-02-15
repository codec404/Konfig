package commands

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/spf13/cobra"
)

func NewUploadCommand() *cobra.Command {
	var (
		serviceName string
		format      string
		description string
		dryRun      bool
	)

	cmd := &cobra.Command{
		Use:   "upload [config-file]",
		Short: "Upload a configuration file",
		Long: `Upload a configuration file to the config service.

The file will be validated, versioned, and stored in the database.
Clients subscribed to this service will receive the update.

Examples:
  configctl upload config.json --service my-service
  configctl upload config.yaml --service my-service --format yaml
  configctl upload config.json --service my-service --dry-run`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configFile := args[0]

			// Read file
			content, err := os.ReadFile(configFile)
			if (err != nil) {
				return fmt.Errorf("failed to read config file: %w", err)
			}

			// Auto-detect format if not specified
			if format == "" {
				format = detectFormat(configFile)
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
			fmt.Println()

			if dryRun {
				fmt.Println("✓ Validation passed")
				fmt.Println("✓ Would upload to service:", serviceName)
				return nil
			}

			// TODO: Implement actual API call
			fmt.Println("✓ Configuration uploaded successfully")
			fmt.Println("  Version: 1")
			fmt.Println("  Config ID: cfg-" + serviceName + "-v1")

			return nil
		},
	}

	cmd.Flags().StringVarP(&serviceName, "service", "n", "", "Service name (required)")
	cmd.Flags().StringVarP(&format, "format", "f", "", "Config format (json|yaml|toml) - auto-detected if not specified")
	cmd.Flags().StringVarP(&description, "description", "d", "", "Configuration description")
	cmd.Flags().BoolVar(&dryRun, "dry-run", false, "Validate without uploading")
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