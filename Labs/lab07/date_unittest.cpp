#include "gtest/gtest.h"
#include "date.h"

class DateTest : public ::testing::Test {
 public:
  void SetUp( ) { 
    // code here will execute just before the test ensues 
	first_day = Date(2026, 1, 20);
	last_day  = Date(2026, 5, 4);
  }
 protected:
  Date first_day;          // first day of classes
  Date last_day;           // last day of classes
};

TEST_F(DateTest, PrintDateTests) {
  Date y2k(1999, 12, 31);              // y2k
  Date ind_day(1776, 7, 4);            // US Independence
  Date best_holiday(2026, 10, 31);     // Halloween
  
  std::string expected_out_1 = "1999-12-31\n";
  std::string expected_out_2 = "1776-07-04\n";
  std::string expected_out_3 = "2026-10-31\n";
  
  testing::internal::CaptureStdout();
  y2k.PrintDate(true);
  std::string output1 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout(); // You must do this EVERY TIME, or else it will segfault
  ind_day.PrintDate(true);
  std::string output2 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout();
  best_holiday.PrintDate(true);
  std::string output3 = testing::internal::GetCapturedStdout();
  
  EXPECT_EQ(output1, expected_out_1);
  EXPECT_EQ(output2, expected_out_2);
  EXPECT_EQ(output3, expected_out_3);
}

TEST_F(DateTest, PrintDateTestsWithoutNewline) {
  Date y2k(1999, 12, 31);              // y2k
  Date ind_day(1776, 7, 4);            // US Independence
  Date best_holiday(2026, 10, 31);     // Halloween
  
  std::string expected_out_1 = "1999-12-31";
  std::string expected_out_2 = "1776-07-04";
  std::string expected_out_3 = "2026-10-31";
  
  testing::internal::CaptureStdout();
  y2k.PrintDate(false);
  std::string output1 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout();
  ind_day.PrintDate(false);
  std::string output2 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout();
  best_holiday.PrintDate(false);
  std::string output3 = testing::internal::GetCapturedStdout();
  
  EXPECT_EQ(output1, expected_out_1);
  EXPECT_EQ(output2, expected_out_2);
  EXPECT_EQ(output3, expected_out_3);
}

TEST_F(DateTest, PrintUsDateTests) {
  Date y2k(1999, 12, 31);              // y2k
  Date ind_day(1776, 7, 4);            // US Independence
  Date best_holiday(2026, 10, 31);     // Halloween
  
  std::string expected_out_1 = "12-31-1999\n";
  std::string expected_out_2 = "07-04-1776\n";
  std::string expected_out_3 = "10-31-2026\n";
  
  testing::internal::CaptureStdout();
  y2k.PrintUsDate(true);
  std::string output1 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout(); // You must do this EVERY TIME, or else it will segfault
  ind_day.PrintUsDate(true);
  std::string output2 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout();
  best_holiday.PrintUsDate(true);
  std::string output3 = testing::internal::GetCapturedStdout();
  
  EXPECT_EQ(output1, expected_out_1);
  EXPECT_EQ(output2, expected_out_2);
  EXPECT_EQ(output3, expected_out_3);
}

TEST_F(DateTest, PrintUsDateTestsWithoutNewLine) {
  Date y2k(1999, 12, 31);              // y2k
  Date ind_day(1776, 7, 4);            // US Independence
  Date best_holiday(2026, 10, 31);     // Halloween
  
  std::string expected_out_1 = "12-31-1999";
  std::string expected_out_2 = "07-04-1776";
  std::string expected_out_3 = "10-31-2026";
  
  testing::internal::CaptureStdout();
  y2k.PrintUsDate(false);
  std::string output1 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout(); // You must do this EVERY TIME, or else it will segfault
  ind_day.PrintUsDate(false);
  std::string output2 = testing::internal::GetCapturedStdout();
  
  testing::internal::CaptureStdout();
  best_holiday.PrintUsDate(false);
  std::string output3 = testing::internal::GetCapturedStdout();
  
  EXPECT_EQ(output1, expected_out_1);
  EXPECT_EQ(output2, expected_out_2);
  EXPECT_EQ(output3, expected_out_3);
}

TEST_F(DateTest, GetDateTests) {
  Date y2k(1999, 12, 31);              // y2k
  Date ind_day(1776, 7, 4);            // US Independence
  Date best_holiday(2026, 10, 31);     // Halloween
  
  std::string expected_out_1 = "1999-12-31";
  std::string expected_out_2 = "1776-07-04";
  std::string expected_out_3 = "2026-10-31";
  
  auto output1 = y2k.GetDate();
  auto output2 = ind_day.GetDate();
  auto output3 = best_holiday.GetDate();
  
  EXPECT_EQ(output1, expected_out_1);
  EXPECT_EQ(output2, expected_out_2);
  EXPECT_EQ(output3, expected_out_3);
}

TEST_F(DateTest, GetUsDateTests) {
  Date y2k(1999, 12, 31);              // y2k
  Date ind_day(1776, 7, 4);            // US Independence
  Date best_holiday(2026, 10, 31);     // Halloween
  
  std::string expected_out_1 = "12-31-1999";
  std::string expected_out_2 = "07-04-1776";
  std::string expected_out_3 = "10-31-2026";
  
  auto output1 = y2k.GetUsDate();
  auto output2 = ind_day.GetUsDate();
  auto output3 = best_holiday.GetUsDate();
  
  EXPECT_EQ(output1, expected_out_1);
  EXPECT_EQ(output2, expected_out_2);
  EXPECT_EQ(output3, expected_out_3);
}

TEST_F(DateTest, ConstructorTestsCurrentDate) {
  Date today = Date(); // Current date
  std::time_t t = std::time(0);
  std::tm* now = std::localtime(&t);
  char expected_out_today[11];
  std::strftime(expected_out_today, 11, "%Y-%m-%d", now);
  auto out_today = today.GetDate();
  EXPECT_EQ(out_today, expected_out_today);
}

TEST_F(DateTest, ConstructorTestsYearMonthDay) {
	// first_day = Date(2026, 1, 20);
  std::string expected_out_first_day = "2026-01-20";
  auto out_first_day = first_day.GetDate();
  EXPECT_EQ(out_first_day, expected_out_first_day);

	// last_day = Date(2026, 5, 4);
  std::string expected_out_last_day = "2026-05-04";
  auto out_last_day = last_day.GetDate();
  EXPECT_EQ(out_last_day, expected_out_last_day);

  // Invalid month, and day values all enumerated
  std::string expected_out_invalid = "1-01-01";
  int invalid_months[] = {-123, 0, 13};
  int invalid_days[]   = {-123, 0, 32};
  for (int month : invalid_months) {
    for (int day : invalid_days) {
      Date invalid_date(2026, month, day);
      auto out_invalid = invalid_date.GetDate();
      EXPECT_EQ(out_invalid, expected_out_invalid);
    }
  }

  // Tricky year values
  int tricky_years[] = {-123, 0, 4000};
  for (int year : tricky_years) {
    Date tricky_date(year, 1, 1);
    auto expected_out_tricky = std::to_string(year) + "-01-01";
    auto out_tricky = tricky_date.GetDate();
    EXPECT_EQ(out_tricky, expected_out_tricky);
  }

  // Leap year tests
  std::vector<std::tuple<Date, std::string>> test_cases = {
    {Date(2020, 2, 29), "2020-02-29"},
    {Date(2004, 2, 29), "2004-02-29"},
    {Date(2000, 2, 29), "2000-02-29"},
    {Date(1200, 2, 29), "1200-02-29"},
    {Date(2021, 2, 29), "1-01-01"},
    {Date(2934, 2, 29), "1-01-01"},
    {Date(1900, 2, 29), "1-01-01"},
    {Date(2100, 2, 29), "1-01-01"},
  };

  for (const auto& test_case : test_cases) {
    const auto &date = std::get<0>(test_case);
    const auto &expected_out = std::get<1>(test_case);
    auto out = date.GetDate();
    EXPECT_EQ(out, expected_out);
  }
}

TEST_F(DateTest, ConstructorTestsEpoch) {
  int epoches[] = {0, 1234567890, -1234567890, std::numeric_limits<int>::max(), std::numeric_limits<int>::min()};
  for (int epoch : epoches) {
    Date epoch_date(epoch);
    std::time_t epoch_t = epoch;
    std::tm* yyyy_mm_dd = std::gmtime( &epoch_t );
    char expected_out_epoch[11];
    std::strftime(expected_out_epoch, 11, "%Y-%m-%d", yyyy_mm_dd);
    auto out_epoch = epoch_date.GetDate();
    EXPECT_EQ(out_epoch, expected_out_epoch);
  }
}

TEST_F(DateTest, AddOperatorTests) {
  std::vector<std::tuple<Date, int, Date>> test_cases = {
    {first_day, 0,    Date(2026, 1, 20)},
    {first_day, 11,   Date(2026, 1, 31)},
    {first_day, -19,  Date(2026, 1, 1)},
    {first_day, 365,  Date(2027, 1, 20)},
    {first_day, -365, Date(2025, 1, 20)},
    {first_day, 12,   Date(2026, 2, 1)},
    {first_day, -20,  Date(2025, 12, 31)},

    // Leap years
    {Date(2020, 2, 29), 1, Date(2020, 3, 1)},
    {Date(2004, 2, 29), 1, Date(2004, 3, 1)},
    {Date(2000, 2, 29), 1, Date(2000, 3, 1)},
    {Date(1200, 2, 29), 1, Date(1200, 3, 1)},
    {Date(2020, 3, 1), -1, Date(2020, 2, 29)},
    {Date(2004, 3, 1), -1, Date(2004, 2, 29)},
    {Date(2000, 3, 1), -1, Date(2000, 2, 29)},
    {Date(1200, 3, 1), -1, Date(1200, 2, 29)},

    // Non-leap years
    {Date(2021, 2, 28), 1, Date(2021, 3, 1)},
    {Date(2934, 2, 28), 1, Date(2934, 3, 1)},
    {Date(1900, 2, 28), 1, Date(1900, 3, 1)},
    {Date(2100, 2, 28), 1, Date(2100, 3, 1)},
    {Date(2021, 3, 1), -1, Date(2021, 2, 28)},
    {Date(2934, 3, 1), -1, Date(2934, 2, 28)},
    {Date(1900, 3, 1), -1, Date(1900, 2, 28)},
    {Date(2100, 3, 1), -1, Date(2100, 2, 28)},
  };

  for (const auto& test_case : test_cases) {
    Date date1 = std::get<0>(test_case);
    int days = std::get<1>(test_case);
    Date expected_date = std::get<2>(test_case);
    Date actual_date = date1 + days;
    EXPECT_EQ(actual_date.GetDate(), expected_date.GetDate());
  }
}

TEST_F(DateTest, SubOperatorTests) {
  std::vector<std::tuple<Date, int, Date>> test_cases = {
    {first_day,  0,   Date(2026, 1, 20)},
    {first_day, -11,  Date(2026, 1, 31)},
    {first_day,  19,  Date(2026, 1, 1)},
    {first_day, -365, Date(2027, 1, 20)},
    {first_day,  365, Date(2025, 1, 20)},
    {first_day, -12,  Date(2026, 2, 1)},
    {first_day,  20,  Date(2025, 12, 31)},

    // Leap years
    {Date(2020, 2, 29), -1, Date(2020, 3, 1)},
    {Date(2004, 2, 29), -1, Date(2004, 3, 1)},
    {Date(2000, 2, 29), -1, Date(2000, 3, 1)},
    {Date(1200, 2, 29), -1, Date(1200, 3, 1)},
    {Date(2020, 3, 1),   1, Date(2020, 2, 29)},
    {Date(2004, 3, 1),   1, Date(2004, 2, 29)},
    {Date(2000, 3, 1),   1, Date(2000, 2, 29)},
    {Date(1200, 3, 1),   1, Date(1200, 2, 29)},

    // Non-leap years
    {Date(2021, 2, 28), -1, Date(2021, 3, 1)},
    {Date(2934, 2, 28), -1, Date(2934, 3, 1)},
    {Date(1900, 2, 28), -1, Date(1900, 3, 1)},
    {Date(2100, 2, 28), -1, Date(2100, 3, 1)},
    {Date(2021, 3, 1),   1, Date(2021, 2, 28)},
    {Date(2934, 3, 1),   1, Date(2934, 2, 28)},
    {Date(1900, 3, 1),   1, Date(1900, 2, 28)},
    {Date(2100, 3, 1),   1, Date(2100, 2, 28)},
  };

  for (const auto& test_case : test_cases) {
    Date date1 = std::get<0>(test_case);
    int days = std::get<1>(test_case);
    Date expected_date = std::get<2>(test_case);
    Date actual_date = date1 - days;
    EXPECT_EQ(actual_date.GetDate(), expected_date.GetDate());
  }
}

TEST_F(DateTest, DaysBetweenTestsExtra) {
  std::vector<std::tuple<Date, Date, int>> test_cases = {
    {first_day, first_day, 0},
    {last_day,  first_day, 104},                   // reversed order
    {Date(2016, 1, 1),   Date(2016, 3, 1), 60},     // leap year
    {Date(2016, 3, 1),   Date(2016, 1, 1), 60},
    {Date(2015, 1, 1),   Date(2015, 3, 1), 59},     // non-leap year
    {Date(2000, 1, 1),   Date(2001, 1, 1), 366},
    {Date(1900, 1, 1),   Date(1901, 1, 1), 365},
    {Date(1999, 12, 31), Date(2000, 1, 1), 1},
  };

  for (const auto& test_case : test_cases) {
    Date date1 = std::get<0>(test_case);
    Date date2 = std::get<1>(test_case);
    int expected_days = std::get<2>(test_case);
    EXPECT_EQ(date1.DaysBetween(date2), expected_days);
  }
}

/**
  *
  * Note, this is the only provided test case which actually uses our test fixture
  * 
  * However, it is illegal to mix TEST() and TEST_F() in the same test case (file).
  *
  */
  
TEST_F(DateTest, DaysBetweenTests) {
  EXPECT_EQ(first_day.GetUsDate(), "01-20-2026") << "First day of class not setup properly";
  EXPECT_EQ(last_day.GetUsDate(), "05-04-2026") << "Last day of class not setup properly";
  EXPECT_EQ(first_day.DaysBetween(last_day), 104) << "Days between is not calculated properly";
}

/**
  *
  * NOPE!  Can't test PRIVATE methods
  *
  */
/*
TEST(DateTest, ConvertFromDays) {
	Date convert_first_day = ConvertToDays(2458365);
	EXPECT_STREQ(convert_first_day.GetUsDate(), "09-04-2018");
}
*/
