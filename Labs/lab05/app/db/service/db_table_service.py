from app.db.dtos import (
	CourseCreate,
	CourseUpdate,
	EnrollmentCreate,
	EnrollmentUpdate,
	StudentCreate,
	StudentUpdate,
)
from app.db.repos.courses import CourseRepo
from app.db.repos.enrollments import EnrollmentRepo
from app.db.repos.students import StudentRepo


class DbTableService:
	def __init__(self, db_pool):
		self.db_pool = db_pool

	def _students(self) -> StudentRepo:
		return StudentRepo(self.db_pool)

	def _courses(self) -> CourseRepo:
		return CourseRepo(self.db_pool)

	def _enrollments(self) -> EnrollmentRepo:
		return EnrollmentRepo(self.db_pool)

	def _grade(self, value):
		return float(value) if value is not None else None

	async def list_students(self):
		rows = await self._students().list_with_enrollments()
		students = {}

		for row in rows:
			student_id = row["student_id"]
			student = students.get(student_id)

			if student is None:
				student = {
					"id": student_id,
					"first_name": row["first_name"],
					"last_name": row["last_name"],
					"email": row["email"],
					"major": row["major"],
					"credits": row["student_credits"],
					"active": row["active"],
					"created_at": row["created_at"],
					"enrollments": [],
				}
				students[student_id] = student

			if row["enrollment_id"] is not None:
				student["enrollments"].append(
					{
						"id": row["enrollment_id"],
						"status": row["status"],
						"grade": self._grade(row["grade"]),
						"enrolled_at": row["enrolled_at"],
						"course": {
							"id": row["course_id"],
							"code": row["course_code"],
							"title": row["course_title"],
							"department": row["course_department"],
							"credits": row["course_credits"],
							"capacity": row["course_capacity"],
						},
					}
				)

		return list(students.values())

	async def list_courses(self):
		rows = await self._courses().list_with_enrollments()
		courses = {}

		for row in rows:
			course_id = row["course_id"]
			course = courses.get(course_id)

			if course is None:
				course = {
					"id": course_id,
					"code": row["code"],
					"title": row["title"],
					"department": row["department"],
					"credits": row["course_credits"],
					"capacity": row["capacity"],
					"enrollments": [],
				}
				courses[course_id] = course

			if row["enrollment_id"] is not None:
				course["enrollments"].append(
					{
						"id": row["enrollment_id"],
						"status": row["status"],
						"grade": self._grade(row["grade"]),
						"enrolled_at": row["enrolled_at"],
						"student": {
							"id": row["student_id"],
							"first_name": row["first_name"],
							"last_name": row["last_name"],
							"email": row["email"],
							"major": row["major"],
							"credits": row["student_credits"],
							"active": row["active"],
							"created_at": row["created_at"],
						},
					}
				)

		return list(courses.values())

	async def list_enrollments(self):
		rows = await self._enrollments().list_with_student_and_course()

		return [
			{
				"id": row["enrollment_id"],
				"status": row["status"],
				"grade": self._grade(row["grade"]),
				"enrolled_at": row["enrolled_at"],
				"student": {
					"id": row["student_id"],
					"first_name": row["first_name"],
					"last_name": row["last_name"],
					"email": row["email"],
					"major": row["major"],
					"credits": row["student_credits"],
					"active": row["active"],
					"created_at": row["created_at"],
				},
				"course": {
					"id": row["course_id"],
					"code": row["course_code"],
					"title": row["course_title"],
					"department": row["course_department"],
					"credits": row["course_credits"],
					"capacity": row["course_capacity"],
				},
			}
			for row in rows
		]

	async def create_student(self, dto: StudentCreate):
		return await self._students().create(dto)

	async def update_student(self, student_id: int, dto: StudentUpdate):
		return await self._students().update(student_id, dto)

	async def create_course(self, dto: CourseCreate):
		return await self._courses().create(dto)

	async def update_course(self, course_id: int, dto: CourseUpdate):
		return await self._courses().update(course_id, dto)

	async def create_enrollment(self, dto: EnrollmentCreate):
		enrollment = await self._enrollments().create(dto)
		enrollment["grade"] = self._grade(enrollment["grade"])
		return enrollment

	async def update_enrollment(self, enrollment_id: int, dto: EnrollmentUpdate):
		enrollment = await self._enrollments().update(enrollment_id, dto)

		if enrollment is not None:
			enrollment["grade"] = self._grade(enrollment["grade"])

		return enrollment
