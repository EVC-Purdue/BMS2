#include <cstdint>
#include <cstddef>


#include "battery/faults.hpp"



namespace faults {

FaultManager::FaultManager() : current_set_faults(0), previous_set_faults(0) {}


uint32_t FaultManager::get_current_set_faults() const {
    return this->current_set_faults;
}


void FaultManager::set_fault(bool condition, size_t fault_bit) {
    if (condition) {
        this->current_set_faults |= (static_cast<uint32_t>(1) << fault_bit);
    }
}

void FaultManager::clear_fault(size_t fault_bit) {
    this->previous_set_faults &= ~(static_cast<uint32_t>(1) << fault_bit);
}

void FaultManager::clear_current_faults() {
    this->current_set_faults = 0;
}

bool FaultManager::new_faults_present() const {
    uint32_t new_faults = this->current_set_faults & ~(this->previous_set_faults);
    return new_faults != 0;
}

void FaultManager::update_previous_faults() {
    this->previous_set_faults |= this->current_set_faults;
}

bool FaultManager::has_fault_active() const {
    uint32_t persistent_faults_mask = (static_cast<uint32_t>(1) << faults::PersistentFault::PERSISTENT_FAULTS_END) - 1;
    uint32_t live_faults_mask = ((static_cast<uint32_t>(1) << faults::LiveFault::LIVE_FAULTS_END) - 1) & ~persistent_faults_mask;
    return ((this->current_set_faults & persistent_faults_mask) != 0) ||  // Check current persistent faults
           ((this->previous_set_faults & persistent_faults_mask) != 0) || // Check previous persistent faults
           ((this->current_set_faults & live_faults_mask) != 0); 		  // Check current live faults
}



} // namespace faults