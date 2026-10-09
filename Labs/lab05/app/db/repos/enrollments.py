from app.db.dtos import EnrollmentCreate, EnrollmentUpdate
from app.db.repos.base import BaseRepo


class EnrollmentRepo(BaseRepo):
	async def list_with_student_and_course(self):
		query = """
			SELECT
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
				s.created_at,
				c.id AS course_id,
				c.code AS course_code,
				c.title AS course_title,
				c.department AS course_department,
				c.credits AS course_credits,
				c.capacity AS course_capacity
			FROM enrollments e
			JOIN students s ON s.id = e.student_id
			JOIN courses c ON c.id = e.course_id
			ORDER BY e.id
		"""

		async with self._connection() as conn:
			rows = await conn.fetch(query)

		return self._rows_dicts(rows)

	async def create(self, dto: EnrollmentCreate):
		query = """
			INSERT INTO enrollments (student_id, course_id, status, grade)
			VALUES ($1, $2, $3, $4)
			RETURNING id, student_id, course_id, status, grade, enrolled_at
		"""

		async with self._connection() as conn:
			row = await conn.fetchrow(
				query,
				dto.student_id,
				dto.course_id,
				dto.status,
				dto.grade,
			)

		return self._row_dict(row)

	async def update(self, enrollment_id: int, dto: EnrollmentUpdate):
		query = """
			UPDATE enrollments
			SET student_id = $2,
				course_id = $3,
				status = $4,
				grade = $5
			WHERE id = $1
			RETURNING id, student_id, course_id, status, grade, enrolled_at
		"""

		async with self._connection() as conn:
			row = await conn.fetchrow(
				query,
				enrollment_id,
				dto.student_id,
				dto.course_id,
				dto.status,
				dto.grade,
			)

		return self._row_dict(row)
