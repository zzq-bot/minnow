#include "reassembler.hh"
#include <algorithm>
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // we need to deal with overlapping intervals in the inserted strings

  uint64_t avail_capacity = output_.writer().available_capacity();
  uint64_t first_unacceptable_index = unassembled_index_ + avail_capacity;
  uint64_t end_index = first_index + data.size();
  // [) interval

  if ( is_last_substring ) {
    eof_idx_ = end_index;
  }

  // 1. discard data beyond capacity and skip if it has already in output_.writer()
  if ( first_index >= first_unacceptable_index || end_index <= unassembled_index_ || data.empty()
       || avail_capacity == 0 ) {
    if ( eof_idx_ == end_index && unassembled_bytes_ == 0 ) {
      output_.writer().close();
    }
    return;
  }

  // 2. cut the substring to fit the capacity and leave out substring already in output_.writer()
  if ( end_index > first_unacceptable_index ) {
    data = data.substr( 0, first_unacceptable_index - first_index );
    end_index = first_unacceptable_index;
  }

  if ( first_index < unassembled_index_ ) {
    data = data.substr( unassembled_index_ - first_index );
    first_index = unassembled_index_;
  }

  // 3. insert the substring into idx2substring_ and merge overlapping intervals
  auto it = idx2substring_.lower_bound( first_index );

  if ( it != idx2substring_.begin() && ( it == idx2substring_.end() || it->first > first_index ) ) {
    --it;
  }

  // merge
  uint64_t new_start = first_index;
  uint64_t new_end = end_index;
  std::string new_data = data;
  while ( it != idx2substring_.end() && it->first <= new_end ) {
    uint64_t existing_start = it->first;
    uint64_t existing_end = existing_start + it->second.size();
    if ( existing_end >= new_start ) {
      uint64_t slice_1 = 0;
      if ( existing_start > new_start ) {
        slice_1 = existing_start - new_start;
      }
      uint64_t slice_2 = std::min( new_data.size(), existing_end - new_start );
      new_data = new_data.substr( 0, slice_1 ) + it->second + new_data.substr( slice_2 );
      new_start = min( new_start, existing_start );
      new_end = max( new_end, existing_end );
      it = idx2substring_.erase( it );
    } else {
      ++it; // targeted on the first block;
    }
  }

  idx2substring_[new_start] = new_data;

  // push "available capacity" data to output_.writer()

  it = idx2substring_.begin();
  if ( it->first == unassembled_index_ ) {
    if ( it->second.size() <= avail_capacity ) {
      output_.writer().push( it->second );
      unassembled_index_ += it->second.size();
      idx2substring_.erase( it );
    } else {
      output_.writer().push( it->second.substr( 0, avail_capacity ) );
      unassembled_index_ += avail_capacity;
      idx2substring_[unassembled_index_] = it->second.substr( avail_capacity );
      idx2substring_.erase( it );
    }
  }
  // re-calculate unassembled_bytes_
  unassembled_bytes_ = 0;
  for ( auto& tmp : idx2substring_ ) {
    unassembled_bytes_ += tmp.second.size();
  }

  std::cout << eof_idx_ << " " << end_index << " " << unassembled_bytes_ << std::endl;
  if ( eof_idx_ == unassembled_index_ && unassembled_bytes_ == 0 ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return unassembled_bytes_; // this will be updated after each insert
}
