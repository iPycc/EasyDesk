#include "data/local/local_transfer_service.h"

void LocalTransferService::start() {
    emit changed();
}

void LocalTransferService::pause() {}

void LocalTransferService::resume() {}

void LocalTransferService::cancel() {}
