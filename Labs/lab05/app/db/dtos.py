from datetime import datetime

from pydantic import BaseModel, ConfigDict


class DTO(BaseModel):
	model_config = ConfigDict(
		extra="forbid",
		from_attributes=True,
		validate_assignment=True,
	)


class IntegerPrimaryKeyMixin(DTO):
	id: int


class CreatedAtMixin(DTO):
	created_at: datetime


class EnrolledAtMixin(DTO):
	enrolled_at: datetime


class StudentBase(DTO):
	first_name: str
	last_name: str
	email: str
	major: str
	credits: int
	active: bool


class StudentCreate(StudentBase):
	active: bool = True


class StudentUpdate(StudentBase):
	pass


class Student(StudentBase, IntegerPrimaryKeyMixin, CreatedAtMixin):
	pass


class CourseBase(DTO):
	code: str
	title: str
	department: str
	credits: int
	capacity: int


class CourseCreate(CourseBase):
	pass


class CourseUpdate(CourseBase):
	pass


class Course(CourseBase, IntegerPrimaryKeyMixin):
	pass


class EnrollmentBase(DTO):
	student_id: int
	course_id: int
	grade: float | None = None


class EnrollmentCreate(EnrollmentBase):
	status: str = "enrolled"


class EnrollmentUpdate(EnrollmentBase):
	status: str


class Enrollment(EnrollmentBase, IntegerPrimaryKeyMixin, EnrolledAtMixin):
	status: str
