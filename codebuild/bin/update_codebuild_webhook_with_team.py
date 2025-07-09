#!/usr/bin/env python3
"""
Script to fetch GitHub team members and add them to AWS CodeBuild webhook filters.
This script requires both GitHub and AWS credentials.
"""

import os
import sys
import json
import boto3
import requests
import click

def setup_dependencies():
    """Install required dependencies using uv"""
    import subprocess
    subprocess.run(["uv", "pip", "install", "requests", "boto3", "click"], check=True)

def fetch_team_members(token, team_slug="s2n-core", org="aws"):
    """Fetch team members from the GitHub API"""
    url = f"https://api.github.com/orgs/{org}/teams/{team_slug}/members"
    headers = {
        "Accept": "application/vnd.github.v3+json",
        "Authorization": f"token {token}",
    }
    
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    
    return response.json()

def extract_member_ids(members):
    """Extract member IDs from the response"""
    return {member["login"]: member["id"] for member in members}

def update_or_create_webhook(project_name, user_ids, event_types=None, aws_region=None, branch_pattern=None):
    """Create or update CodeBuild webhook configuration with GitHub user IDs"""
    if event_types is None:
        event_types = ["PULL_REQUEST_CREATED", "PULL_REQUEST_UPDATED"]
    # Ensure event_types is a list
    if isinstance(event_types, str):
        event_types = [et.strip() for et in event_types.split(',') if et.strip()]
    if branch_pattern is None:
        branch_pattern = "^refs/heads/.*"  # Match all branches by default
    
    # Join event types as a regex pattern using | (OR operator)
    event_pattern = ",".join(event_types)
    
    session = boto3.Session(region_name=aws_region)
    codebuild = session.client('codebuild')
    
    # First, get the current project configuration
    try:
        response = codebuild.batch_get_projects(names=[project_name])
        
        if not response['projects']:
            click.echo(f"Error: Project '{project_name}' not found.", err=True)
            return False
            
        project = response['projects'][0]
        
        # Create a single filter group
        filter_group = []
        
        # Add the combined event filter
        event_filter = {
            'type': 'EVENT',
            'pattern': event_pattern,
            'excludeMatchedPattern': False
        }
        filter_group.append(event_filter)
        
        # Add user ID filters
        for user_login, user_id in user_ids.items():
            user_filter = {
                'type': 'ACTOR_ACCOUNT_ID',
                'pattern': str(user_id),
                'excludeMatchedPattern': False
            }
            filter_group.append(user_filter)
        
        # Create filter groups list with just the one filter group
        filter_groups = [filter_group]
        
        # Debug information
        click.echo("\nFilter group configuration:")
        click.echo(json.dumps(filter_groups, indent=2))
        
        # Check if webhook is already configured
        if 'webhook' in project and project['webhook'].get('url'):
            # Webhook exists, update it
            click.echo(f"Updating existing webhook for '{project_name}'...")
            response = codebuild.update_webhook(
                projectName=project_name,
                filterGroups=filter_groups
            )
            action = "Updated"
        else:
            # Webhook doesn't exist, create it
            click.echo(f"Creating new webhook for '{project_name}'...")
            
            # Add HEAD_REF filter to the filter group for branch matching
            # when creating a new webhook (instead of using branchFilter)
            filter_group.append({
                'type': 'HEAD_REF',
                'pattern': branch_pattern,
                'excludeMatchedPattern': False
            })
            
            response = codebuild.create_webhook(
                projectName=project_name,
                filterGroups=filter_groups
            )
            action = "Created"
        
        click.echo(f"Successfully {action.lower()} webhook configuration for '{project_name}'")
        click.echo(f"Added {len(user_ids)} users to the webhook filters for events: {', '.join(event_types)}")
        
        # Display the webhook URL if available
        if 'webhook' in response and 'url' in response['webhook']:
            click.echo(f"Webhook URL: {response['webhook']['url']}")
        
        return True
        
    except Exception as e:
        click.echo(f"Error updating webhook configuration: {e}", err=True)
        return False

@click.command()
@click.option('--token', envvar='GITHUB_TOKEN', help='GitHub API token')
@click.option('--team', default='s2n-core', help='GitHub team slug')
@click.option('--org', default='aws', help='GitHub organization')
@click.option('--project-name', help='AWS CodeBuild project name')
@click.option('--aws-region', help='AWS region (defaults to AWS_REGION env var or profile)')
@click.option('--fetch-only', is_flag=True, help='Only fetch team members without updating CodeBuild')
@click.option('--event-types', default='PULL_REQUEST_CREATED,PULL_REQUEST_UPDATED', 
              help='Comma-separated list of event types (PUSH, PULL_REQUEST_CREATED, PULL_REQUEST_UPDATED, PULL_REQUEST_MERGED, etc.)')
@click.option('--branch-pattern', default='^refs/heads/.*', 
              help='Branch pattern for new webhooks (e.g. "^refs/heads/main$" for main branch only)')
def main(token, team, org, project_name, aws_region, fetch_only, event_types, branch_pattern):
    """Fetch GitHub team members and update CodeBuild webhook configuration"""
    if not token:
        click.echo("GitHub token required. Set GITHUB_TOKEN environment variable or use --token")
        sys.exit(1)
    
    # Parse event types
    event_type_list = [et.strip() for et in event_types.split(',') if et.strip()]
    
    try:
        # Fetch team members
        click.echo(f"Fetching members from {org}/{team}...")
        members = fetch_team_members(token, team, org)
        member_ids = extract_member_ids(members)
        
        # Output the team member information
        click.echo("Team members:")
        click.echo(json.dumps(member_ids, indent=2))
        click.echo(f"Total members: {len(member_ids)}")
        
        # If fetch-only flag is set or no project name provided, stop here
        if fetch_only or not project_name:
            if not fetch_only and not project_name:
                click.echo("\nNo CodeBuild project name provided. Use --project-name to update webhook.")
            return
        
        # Update or create CodeBuild webhook
        click.echo(f"\nConfiguring CodeBuild project '{project_name}' webhook...")
        click.echo(f"Using event types: {', '.join(event_type_list)}")
        click.echo(f"Using branch pattern: {branch_pattern}")
        success = update_or_create_webhook(project_name, member_ids, event_type_list, aws_region, branch_pattern)
        
        if not success:
            sys.exit(1)
            
    except requests.exceptions.RequestException as e:
        click.echo(f"Error fetching team members: {e}", err=True)
        sys.exit(1)
    except Exception as e:
        click.echo(f"Error: {e}", err=True)
        sys.exit(1)

if __name__ == "__main__":
    setup_dependencies()
    main()
