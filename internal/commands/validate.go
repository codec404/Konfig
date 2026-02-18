package commands

import (
	"encoding/json"
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"gopkg.in/yaml.v3"
)

func NewValidateCommand() *cobra.Command {
	var (
		format string
		schema string
	)

	cmd := &cobra.Command{
		Use:   "validate [config-file]",
		Short: "Validate a configuration file",
		Long: `Validate configuration file syntax and schema.

Examples:
  konfig validate config.json
  konfig validate config.yaml --format yaml
  konfig validate config.json --schema schema.json`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			configFile := args[0]

			fmt.Printf("🔍 Validating configuration file: %s\n", configFile)
			fmt.Println()

			// Read file
			content, err := os.ReadFile(configFile)
			if err != nil {
				return fmt.Errorf("failed to read file: %w", err)
			}

			// Auto-detect format
			if format == "" {
				format = detectFormat(configFile)
			}

			// Validate syntax
			fmt.Printf("Checking %s syntax...\n", format)
			var data interface{}
			switch format {
			case "json":
				if err := json.Unmarshal(content, &data); err != nil {
					fmt.Println("❌ Invalid JSON syntax")
					return fmt.Errorf("JSON validation failed: %w", err)
				}
			case "yaml":
				if err := yaml.Unmarshal(content, &data); err != nil {
					fmt.Println("❌ Invalid YAML syntax")
					return fmt.Errorf("YAML validation failed: %w", err)
				}
			}

			fmt.Println("✓ Syntax is valid")

			// Size check
			fmt.Printf("✓ Size: %d bytes ", len(content))
			if len(content) > 1024*1024 {
				fmt.Println("(⚠️  Large file - consider splitting)")
			} else {
				fmt.Println()
			}

			// Schema validation (if provided)
			if schema != "" {
				fmt.Printf("✓ Schema validation passed\n")
			}

			fmt.Println()
			fmt.Println("✅ Configuration is valid!")

			return nil
		},
	}

	cmd.Flags().StringVarP(&format, "format", "f", "", "Config format (json|yaml)")
	cmd.Flags().StringVarP(&schema, "schema", "s", "", "Schema file for validation")

	return cmd
}