from __future__ import annotations

import os
from typing import AsyncIterator

import asyncpg # type: ignore
from fastapi import Request


DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = int(os.getenv("DB_PORT", "5432"))
DB_NAME = os.getenv("DB_NAME", "paviml_test")
DB_USER = os.getenv("DB_USER", "postgres")
DB_PASSWORD = os.getenv("DB_PASSWORD", "postgres")


async def create_pool(params=None) -> asyncpg.Pool:
	if params is not None:
		DB_HOST = params.get("DB_HOST", "localhost")
		DB_PORT = int(params.get("DB_PORT", "5432"))
		DB_NAME = params.get("DB_NAME", "paviml_test")
		DB_USER = params.get("DB_USER", "postgres")
		DB_PASSWORD = params.get("DB_PASSWORD", "postgres")

	return await asyncpg.create_pool(
		host=DB_HOST, # type: ignore
		port=DB_PORT, # type: ignore
		database=DB_NAME, # type: ignore
		user=DB_USER, # type: ignore
		password=DB_PASSWORD, # type: ignore
		min_size=1,
		max_size=10,
		command_timeout=30,
	)


async def close_pool(pool: asyncpg.Pool) -> None:
    await pool.close()


def get_pool(request: Request) -> asyncpg.Pool:
    pool = getattr(request.app.state, "db_pool", None)

    if pool is None:
        raise RuntimeError("Database pool has not been initialized")

    return pool