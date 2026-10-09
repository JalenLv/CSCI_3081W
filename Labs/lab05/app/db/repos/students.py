from app.db.dtos import StudentCreate, StudentUpdate
from app.db.repos.base import BaseRepo


class StudentRepo(BaseRepo):
	async def list_with_enrollments(self):
		query = """
			SELECT
				s.id AS student_id,
				s.first_name,
				s.last_name,
				s.email,
				s.major,
				s.credits AS student_credits,
				s.active,
				s.created_at,
				e.id AS enrollment_id,
				e.status,
				e.grade,
				e.enrolled_at,
				c.id AS course_id,
				c.code AS course_code,
				c.title AS course_title,
				c.department AS course_department,
				c.credits AS course_credits,
				c.capacity AS course_capacity
			FROM students s
			LEFT JOIN enrollments e ON e.student_id = s.id
			LEFT JOIN courses c ON c.id = e.course_id
			ORDER BY s.id, c.code
		"""

		async with self._connection() as conn:
			rows = await conn.fetch(query)

		return self._rows_dicts(rows)

	async def create(self, dto: StudentCreate):
		query = """
			INSERT INTO students (first_name, last_name, email, major, credits, active)
			VALUES ($1, $2, $3, $4, $5, $6)
			RETURNING id, first_name, last_name, email, major, credits, active, created_at
		"""

		async with self._connection() as conn:
			row = await conn.fetchrow(
				query,
				dto.first_name,
				dto.last_name,
				dto.email,
				dto.major,
				dto.credits,
				dto.active,
			)

		return self._row_dict(row)

	async def update(self, student_id: int, dto: StudentUpdate):
		query = """
			UPDATE students
			SET first_name = $2,
				last_name = $3,
				email = $4,
				major = $5,
				credits = $6,
				active = $7
			WHERE id = $1
			RETURNING id, first_name, last_name, email, major, credits, active, created_at
		"""

		async with self._connection() as conn:
			row = await conn.fetchrow(
				query,
				student_id,
				dto.first_name,
				dto.last_name,
				dto.email,
				dto.major,
				dto.credits,
				dto.active,
			)

		return self._row_dict(row)
