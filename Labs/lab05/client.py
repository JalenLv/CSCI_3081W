import argparse
import json
import sys

import httpx


DEFAULT_BASE_URL = "http://localhost:8000"


def add_student_fields(parser: argparse.ArgumentParser, *, include_active: bool) -> None:
    parser.add_argument("--first-name", required=True)
    parser.add_argument("--last-name", required=True)
    parser.add_argument("--email", required=True)
    parser.add_argument("--major", required=True)
    parser.add_argument("--credits", required=True, type=int)
    if include_active:
        parser.add_argument("--active", choices=["true", "false"], required=True)


def student_payload(args: argparse.Namespace) -> dict:
    payload = {
        "first_name": args.first_name,
        "last_name": args.last_name,
        "email": args.email,
        "major": args.major,
        "credits": args.credits,
    }
    if hasattr(args, "active"):
        payload["active"] = args.active == "true"
    return payload


def print_response(response: httpx.Response) -> None:
    try:
        body = response.json()
    except json.JSONDecodeError:
        body = {"status_code": response.status_code, "text": response.text}

    print(json.dumps(body, indent=2, sort_keys=True))
    response.raise_for_status()


def run(args: argparse.Namespace) -> None:
    base_url = args.base_url.rstrip("/")
    path = f"/db/{args.resource}"
    payload = None

    if args.action == "list":
        method = "GET"
    elif args.action == "create":
        method = "POST"
        payload = student_payload(args)
    elif args.action == "update":
        method = "PUT"
        path = f"{path}/{args.id}"
        payload = student_payload(args)
    elif args.action == "delete":
        method = "DELETE"
        path = f"{path}/{args.id}"
    else:
        raise ValueError(f"Unsupported action: {args.action}")

    with httpx.Client(base_url=base_url, timeout=10.0) as client:
        response = client.request(method, path, json=payload)
        print_response(response)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Lab 05 API client")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)

    resources = parser.add_subparsers(dest="resource", required=True)

    students = resources.add_parser("students")
    student_actions = students.add_subparsers(dest="action", required=True)
    student_actions.add_parser("list")

    create_student = student_actions.add_parser("create")
    add_student_fields(create_student, include_active=False)

    update_student = student_actions.add_parser("update")
    update_student.add_argument("id", type=int)
    add_student_fields(update_student, include_active=True)

    delete_student = student_actions.add_parser("delete")
    delete_student.add_argument("id", type=int)

    for resource in ("courses", "enrollments"):
        resource_parser = resources.add_parser(resource)
        actions = resource_parser.add_subparsers(dest="action", required=True)
        actions.add_parser("list")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        run(args)
    except httpx.HTTPStatusError as exc:
        return 1 if exc.response.status_code < 500 else 2
    except httpx.HTTPError as exc:
        print(json.dumps({"error": str(exc)}, indent=2), file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
