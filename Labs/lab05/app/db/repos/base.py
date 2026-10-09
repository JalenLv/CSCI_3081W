from contextlib import asynccontextmanager
from typing import Any, AsyncIterator


class BaseRepo:
	def __init__(self, db_pool, conn=None):
		self.db = db_pool
		self.conn = conn

	@asynccontextmanager
	async def _connection(self) -> AsyncIterator[Any]:
		if self.conn is not None:
			yield self.conn
			return

		async with self.db.acquire() as conn:
			yield conn

	def _row_dict(self, row):
		if row is None:
			return None

		return dict(row)

	def _rows_dicts(self, rows):
		return [dict(row) for row in rows]
