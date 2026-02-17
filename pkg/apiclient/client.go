package apiclient

import (
	"context"
	"fmt"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	pb "github.com/codec404/Konfig/pkg/pb"
)

// Client wraps the gRPC client for API service
type Client struct {
	conn   *grpc.ClientConn
	client pb.ConfigAPIServiceClient
}

// NewClient creates a new API client
func NewClient(serverAddr string) (*Client, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, err := grpc.DialContext(ctx, serverAddr,
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithBlock())
	if err != nil {
		return nil, fmt.Errorf("failed to connect to %s: %w", serverAddr, err)
	}

	return &Client{
		conn:   conn,
		client: pb.NewConfigAPIServiceClient(conn),
	}, nil
}

// Close closes the client connection
func (c *Client) Close() error {
	if c.conn != nil {
		return c.conn.Close()
	}
	return nil
}

// UploadConfig uploads a configuration
func (c *Client) UploadConfig(ctx context.Context, req *pb.UploadConfigRequest) (*pb.UploadConfigResponse, error) {
	return c.client.UploadConfig(ctx, req)
}

// GetConfig gets a configuration by ID
func (c *Client) GetConfig(ctx context.Context, configID string) (*pb.GetConfigResponse, error) {
	return c.client.GetConfig(ctx, &pb.GetConfigRequest{
		ConfigId: configID,
	})
}

// ListConfigs lists configurations for a service
func (c *Client) ListConfigs(ctx context.Context, serviceName string, limit, offset int32) (*pb.ListConfigsResponse, error) {
	return c.client.ListConfigs(ctx, &pb.ListConfigsRequest{
		ServiceName: serviceName,
		Limit:       limit,
		Offset:      offset,
	})
}

// DeleteConfig deletes a configuration
func (c *Client) DeleteConfig(ctx context.Context, configID string) (*pb.DeleteConfigResponse, error) {
	return c.client.DeleteConfig(ctx, &pb.DeleteConfigRequest{
		ConfigId: configID,
	})
}

// StartRollout starts a rollout
func (c *Client) StartRollout(ctx context.Context, req *pb.StartRolloutRequest) (*pb.StartRolloutResponse, error) {
	return c.client.StartRollout(ctx, req)
}

// GetRolloutStatus gets rollout status
func (c *Client) GetRolloutStatus(ctx context.Context, configID string) (*pb.GetRolloutStatusResponse, error) {
	return c.client.GetRolloutStatus(ctx, &pb.GetRolloutStatusRequest{
		ConfigId: configID,
	})
}

// Rollback rolls back to a previous version
func (c *Client) Rollback(ctx context.Context, serviceName string, targetVersion int64) (*pb.RollbackResponse, error) {
	return c.client.Rollback(ctx, &pb.RollbackRequest{
		ServiceName:   serviceName,
		TargetVersion: targetVersion,
	})
}