from fastapi import APIRouter, Depends, HTTPException

from app.db.dtos import (
	CourseCreate,
	CourseUpdate,
	EnrollmentCreate,
	EnrollmentUpdate,
	StudentCreate,
	StudentUpdate,
)
from app.db.service.db_table_service import DbTableService
from app.deps.db_pool import get_pool

router = APIRouter(prefix="/db", tags=["db"])


def get_db_table_service(pool=Depends(get_pool)) -> DbTableService:
	return DbTableService(pool)


@router.get("/students")
async def read_students(service: DbTableService = Depends(get_db_table_service)):
	return await service.list_students()


@router.get("/courses")
async def read_courses(service: DbTableService = Depends(get_db_table_service)):
	return await service.list_courses()


@router.get("/enrollments")
async def read_enrollments(service: DbTableService = Depends(get_db_table_service)):
	return await service.list_enrollments()


@router.post("/students", status_code=201)
async def create_student(
	student: StudentCreate,
	service: DbTableService = Depends(get_db_table_service),
):
	return await service.create_student(student)


@router.put("/students/{student_id}")
async def update_student(
	student_id: int,
	student: StudentUpdate,
	service: DbTableService = Depends(get_db_table_service),
):
	row = await service.update_student(student_id, student)

	if row is None:
		raise HTTPException(status_code=404, detail="Student not found")

	return row


@router.post("/courses", status_code=201)
async def create_course(
	course: CourseCreate,
	service: DbTableService = Depends(get_db_table_service),
):
	return await service.create_course(course)


@router.put("/courses/{course_id}")
async def update_course(
	course_id: int,
	course: CourseUpdate,
	service: DbTableService = Depends(get_db_table_service),
):
	row = await service.update_course(course_id, course)

	if row is None:
		raise HTTPException(status_code=404, detail="Course not found")

	return row


@router.post("/enrollments", status_code=201)
async def create_enrollment(
	enrollment: EnrollmentCreate,
	service: DbTableService = Depends(get_db_table_service),
):
	return await service.create_enrollment(enrollment)


@router.put("/enrollments/{enrollment_id}")
async def update_enrollment(
	enrollment_id: int,
	enrollment: EnrollmentUpdate,
	service: DbTableService = Depends(get_db_table_service),
):
	row = await service.update_enrollment(enrollment_id, enrollment)

	if row is None:
		raise HTTPException(status_code=404, detail="Enrollment not found")

	return row
