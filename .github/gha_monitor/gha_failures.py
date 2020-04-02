#!/usr/bin/env python3

import boto3
import logging
import os

from modules import github
from modules import sns
from datetime import datetime, timedelta
from dateutil import parser, tz

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
# NOTE: this should roughly match the times for cron in the workflow yaml.
# e.g. if cron is set to run every hour, give some wiggle room and use hours=1.1
# TODO: value is for testing- change before PR
TIME_WINDOW_BEGIN = datetime.utcnow().astimezone(tz.UTC) - timedelta(hours=.25)
TIME_WINDOW_END = datetime.utcnow().astimezone(tz.UTC)
ONCALL_WIKI = "https://w.amazon.com/bin/view/AWSCryptography/AWSCryptoTools/TransportLibraries/OnCall/"


class GitHub_Actions(github.GitHub_Client):
    params = {
        # Needed when using an API key
        'github_username': os.getenv('github_username'),
        'github_password': os.getenv('github_password'),
        # Use from within an Action
        'token': os.getenv('GITHUB_TOKEN'),
        'repo_organization': os.getenv('GITHUB_REPO_ORG'),
        'repo': os.getenv('GITHUB_REPO')
    }


class S2n_Notices(sns.SNS_Client):
    params = {
        'topic_arn': 'arn:aws:sns:us-west-2:024603541914:s2n_notices'
    }


def message_text():
    return """
s2n-notice
State: failed
Repo: {repo}
GHA failure time: {time}
Workflow name: {workflow_name}
Wiki: {wiki}
URL: {url}
started by: {commit_owner}\n
"""


def workflow_filter(log):
    """ Reduce the number of elements."""
    top_level_keys = set(['head_branch', 'head_commit', 'status', 'conclusion', 'html_url', 'created_at', 'repository'])
    repository_keys = set([''])
    to_drop = set(log) - top_level_keys

    for element in log:
        del log[element]


def main():
    """ Main entrypoint. """
    logging.info('Starting up')
    notices = []
    gh_api = GitHub_Actions()
    sns_client = S2n_Notices()

    # Get the Action workflow log from the Github API
    gh_api.get_workflow_log_chunk(final_state='failure')
    logging.info(f"Looking for events newer than {TIME_WINDOW_BEGIN}")
    for i in gh_api.worklog:
        created_at = parser.parse(i['created_at'])
        logging.debug(f"looking at event from {created_at}")
        if created_at > TIME_WINDOW_BEGIN:
            logging.debug("Workflow_url: " + i['workflow_url'])
            # The name of the workflow isn't in the failure object, look it up.
            workflow_name = gh_api.get_workflow_name(i['workflow_url'].split('/')[-1:][0])
            # Construct a notification string.
            notice_msg = message_text().format(
                time=i['created_at'], \
                url=i['html_url'], \
                commit_owner=i['head_commit']['author']['email'], \
                repo=gh_api.params['repo'], \
                wiki=ONCALL_WIKI, \
                workflow_name=workflow_name)
            logging.debug(notice_msg)
            notices.append(notice_msg)

    # Relay messages to SNS
    for i in notices:
        logging.info(sns_client.publish(i))
    logging.info('Done.')
    return gh_api.response


if __name__ == '__main__':
    main()
