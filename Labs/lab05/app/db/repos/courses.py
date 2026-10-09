from app.db.dtos import CourseCreate, CourseUpdate
from app.db.repos.base import BaseRepo


class CourseRepo(BaseRepo):
	async def list_with_enrollments(self):
		query = """
			SELECT
				c.id AS course_id,
				c.code,
				c.title,
				c.department,
				c.credits AS course_credits,
				c.capacity,
				e.id AS enrollment_id,
				e.status,
				e.grade,
				e.enrolled_at,
				s.id AS student_id,
				s.first_name,
				s.last_name,
				s.email,
				s.major,
				s.credits AS student_credits,
				s.active,
				s.created_at
			FROM courses c
			LEFT JOIN enrollments e ON e.course_id = c.id
			LEFT JOIN students s ON s.id = e.student_id
			ORDER BY c.code, s.last_name, s.first_name
		"""

		async with self._connection() as conn:
			rows = await conn.fetch(query)

		return self._rows_dicts(rows)

	async def create(self, dto: CourseCreate):
		query = """
			INSERT INTO courses (code, title, department, credits, capacity)
			VALUES ($1, $2, $3, $4, $5)
			RETURNING id, code, title, department, credits, capacity
		"""

		async with self._connection() as conn:
			row = await conn.fetchrow(
				query,
				dto.code,
				dto.title,
				dto.department,
				dto.credits,
				dto.capacity,
			)

		return self._row_dict(row)

	async def update(self, course_id: int, dto: CourseUpdate):
		query = """
			UPDATE courses
			SET code = $2,
				title = $3,
				department = $4,
				credits = $5,
				capacity = $6
			WHERE id = $1
			RETURNING id, code, title, department, credits, capacity
		"""

		async with self._connection() as conn:
			row = await conn.fetchrow(
				query,
				course_id,
				dto.code,
				dto.title,
				dto.department,
				dto.credits,
				dto.capacity,
			)

		return self._row_dict(row)
