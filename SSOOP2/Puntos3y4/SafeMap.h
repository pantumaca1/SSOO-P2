#pragma once

class SafeMap{
  public:
    explicit SafeMap(std::string_view sv) noexcept : sv_{sv}{}
    explicit SafeMap() noexcept : sv_{}{};
    SafeMap(const SafeMap&) = delete;
    SafeMap& operator=(const SafeMap&) = delete;
    SafeMap(SafeMap&& other) noexcept : sv_{other.sv_}{
      other.sv_ = {};
    }
    SafeMap& operator=(SafeMap&& other) noexcept{
      if(this != &other && sv_ != other.sv_){
        if(!sv_.empty()) munmap(const_cast<char*>(sv_.data()), sv_.size());
        sv_ = other.sv_;
        other.sv_ = {};
      }
      return *this;
    }
    ~SafeMap() noexcept{
      if(!sv_.empty()) munmap(const_cast<char*>(sv_.data()), sv_.size());
    }
    [[nodiscard]] std::string_view get() const noexcept{
      return sv_;
    }
  private:
    std::string_view sv_;
};
