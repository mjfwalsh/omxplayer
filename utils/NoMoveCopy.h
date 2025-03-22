#pragma once

class NoMoveCopy
{
public:
  NoMoveCopy() = default;

protected:
  NoMoveCopy(const NoMoveCopy &other) = delete;
  NoMoveCopy &operator=(const NoMoveCopy &other) = delete;
  NoMoveCopy &operator=(NoMoveCopy &&other) noexcept = delete;
  NoMoveCopy(NoMoveCopy &&other) noexcept = delete;
};
