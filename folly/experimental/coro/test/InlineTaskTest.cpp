/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/Portability.h>

#if FOLLY_HAS_COROUTINES

#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/detail/InlineTask.h>
#include <folly/portability/GTest.h>

#include <tuple>

template <typename T>
using InlineTask = folly::coro::detail::InlineTask<T>;

class InlineTaskTest : public testing::Test {};

TEST_F(InlineTaskTest, CallVoidTaskWithoutAwaitingNeverRuns) {
  bool hasStarted = false;
  auto f = [&]() -> InlineTask<void> {
    hasStarted = true;
    co_return;
  };
  {
    auto task = f();
    EXPECT_FALSE(hasStarted);
  }
  EXPECT_FALSE(hasStarted);
}

TEST_F(InlineTaskTest, CallValueTaskWithoutAwaitingNeverRuns) {
  bool hasStarted = false;
  auto f = [&]() -> InlineTask<int> {
    hasStarted = true;
    co_return 123;
  };
  {
    auto task = f();
    EXPECT_FALSE(hasStarted);
  }
  EXPECT_FALSE(hasStarted);
}

TEST_F(InlineTaskTest, CallRefTaskWithoutAwaitingNeverRuns) {
  bool hasStarted = false;
  int value;
  auto f = [&]() -> InlineTask<int&> {
    hasStarted = true;
    co_return value;
  };
  {
    auto task = f();
    EXPECT_FALSE(hasStarted);
  }
  EXPECT_FALSE(hasStarted);
}

TEST_F(InlineTaskTest, SimpleVoidTask) {
  bool hasRun = false;
  auto f = [&]() -> InlineTask<void> {
    hasRun = true;
    co_return;
  };
  auto t = f();
  EXPECT_FALSE(hasRun);
  folly::coro::blockingWait(std::move(t));
  EXPECT_TRUE(hasRun);
}

TEST_F(InlineTaskTest, SimpleValueTask) {
  bool hasRun = false;
  auto f = [&]() -> InlineTask<int> {
    hasRun = true;
    co_return 42;
  };
  auto t = f();
  EXPECT_FALSE(hasRun);
  EXPECT_EQ(42, folly::coro::blockingWait(std::move(t)));
  EXPECT_TRUE(hasRun);
}

TEST_F(InlineTaskTest, SimpleRefTask) {
  bool hasRun = false;
  auto f = [&]() -> InlineTask<bool&> {
    hasRun = true;
    co_return hasRun;
  };

  auto t = f();
  EXPECT_FALSE(hasRun);
  auto& result = folly::coro::blockingWait(std::move(t));
  EXPECT_TRUE(hasRun);
  EXPECT_EQ(&hasRun, &result);
}

struct MoveOnlyType {
  int value_;

  explicit MoveOnlyType(int value) noexcept : value_(value) {}

  MoveOnlyType(MoveOnlyType&& other) noexcept
      : value_(std::exchange(other.value_, -1)) {}

  MoveOnlyType& operator=(MoveOnlyType&& other) noexcept {
    value_ = std::exchange(other.value_, -1);
    return *this;
  }

  ~MoveOnlyType() {
    value_ = -2;
  }
};

struct TypeWithImplicitSingleValueConstructor {
  float value_;
  /* implicit */ TypeWithImplicitSingleValueConstructor(float x) : value_(x) {}
};

TEST_F(InlineTaskTest, ReturnValueWithInitializerListSyntax) {
  auto f = []() -> InlineTask<TypeWithImplicitSingleValueConstructor> {
    co_return{1.23f};
  };

  auto result = folly::coro::blockingWait(f());
  EXPECT_EQ(1.23f, result.value_);
}

struct TypeWithImplicitMultiValueConstructor {
  std::string s_;
  float x_;
  /* implicit */ TypeWithImplicitMultiValueConstructor(
      std::string s,
      float x) noexcept
      : s_(s), x_(x) {}
};

TEST_F(InlineTaskTest, ReturnValueWithInitializerListSyntax2) {
  auto f = []() -> InlineTask<TypeWithImplicitMultiValueConstructor> {
    co_return{"hello", 3.1415f};
  };

  auto result = folly::coro::blockingWait(f());
  EXPECT_EQ("hello", result.s_);
  EXPECT_EQ(3.1415f, result.x_);
}

TEST_F(InlineTaskTest, TaskOfMoveOnlyType) {
  auto f = []() -> InlineTask<MoveOnlyType> { co_return MoveOnlyType{42}; };

  auto x = folly::coro::blockingWait(f());
  EXPECT_EQ(42, x.value_);

  bool executed = false;
  auto g = [&]() -> InlineTask<void> {
    auto result = co_await f();
    EXPECT_EQ(42, result.value_);
    executed = true;
  };

  folly::coro::blockingWait(g());

  EXPECT_TRUE(executed);
}

TEST_F(InlineTaskTest, MoveOnlyTypeNRVO) {
  auto f = []() -> InlineTask<MoveOnlyType> {
    MoveOnlyType x{10};
    co_return x;
  };

  auto x = folly::coro::blockingWait(f());
  EXPECT_EQ(10, x.value_);
}

TEST_F(InlineTaskTest, ReturnLvalueReference) {
  int value = 0;
  auto f = [&]() -> InlineTask<int&> { co_return value; };

  auto& x = folly::coro::blockingWait(f());
  EXPECT_EQ(&value, &x);
}

struct MyException : std::exception {};

TEST_F(InlineTaskTest, ExceptionsPropagateFromVoidTask) {
  auto f = []() -> InlineTask<void> {
    co_await std::experimental::suspend_never{};
    throw MyException{};
  };
  EXPECT_THROW(folly::coro::blockingWait(f()), MyException);
}

TEST_F(InlineTaskTest, ExceptionsPropagateFromValueTask) {
  auto f = []() -> InlineTask<int> {
    co_await std::experimental::suspend_never{};
    throw MyException{};
  };
  EXPECT_THROW(folly::coro::blockingWait(f()), MyException);
}

TEST_F(InlineTaskTest, ExceptionsPropagateFromRefTask) {
  auto f = []() -> InlineTask<int&> {
    co_await std::experimental::suspend_never{};
    throw MyException{};
  };
  EXPECT_THROW(folly::coro::blockingWait(f()), MyException);
}

struct ThrowingCopyConstructor {
  ThrowingCopyConstructor() noexcept = default;

  [[noreturn]] ThrowingCopyConstructor(const ThrowingCopyConstructor&) noexcept(
      false) {
    throw MyException{};
  }

  ThrowingCopyConstructor& operator=(const ThrowingCopyConstructor&) = delete;
};

TEST_F(InlineTaskTest, ExceptionsPropagateFromReturnValueConstructor) {
  auto f = []() -> InlineTask<ThrowingCopyConstructor> { co_return{}; };
  EXPECT_THROW(folly::coro::blockingWait(f()), MyException);
}

InlineTask<void> recursiveTask(int depth) {
  if (depth > 0) {
    co_await recursiveTask(depth - 1);
  }
}

TEST_F(InlineTaskTest, DeepRecursionDoesntStackOverflow) {
  folly::coro::blockingWait(recursiveTask(500000));
}

InlineTask<int> recursiveValueTask(int depth) {
  if (depth > 0) {
    co_return co_await recursiveValueTask(depth - 1) + 1;
  }
  co_return 0;
}

TEST_F(InlineTaskTest, DeepRecursionOfValueTaskDoesntStackOverflow) {
  EXPECT_EQ(500000, folly::coro::blockingWait(recursiveValueTask(500000)));
}

InlineTask<void> recursiveThrowingTask(int depth) {
  if (depth > 0) {
    co_await recursiveThrowingTask(depth - 1);
  }

  throw MyException{};
}

TEST_F(InlineTaskTest, DeepRecursionOfExceptions) {
  EXPECT_THROW(
      folly::coro::blockingWait(recursiveThrowingTask(50000)), MyException);
}

#endif
