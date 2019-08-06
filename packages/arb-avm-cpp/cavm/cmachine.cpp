/*
 * Copyright 2019, Offchain Labs, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cmachine.h"

#include <avm/machine.hpp>

#include <sys/stat.h>
#include <fstream>
#include <iostream>

typedef struct {
    uint64_t stepCount;
} cassertion;

Machine* read_files(std::string filename) {
    std::ifstream myfile;

    struct stat filestatus;
    stat(filename.c_str(), &filestatus);

    char* buf = (char*)malloc(filestatus.st_size);

    myfile.open(filename, std::ios::in);
    if (myfile.is_open()) {
        myfile.read((char*)buf, filestatus.st_size);
        myfile.close();
    }
    auto machine = new Machine();
    machine->deserialize(buf);
    return machine;
}

// cmachine_t *machine_create(char *data)
CMachine* machineCreate(const char* filename) {
    Machine* mach = read_files(filename);
    return static_cast<void*>(mach);
}

void machineDestroy(CMachine* m) {
    if (m == NULL)
        return;
    delete static_cast<Machine*>(m);
}

void machineHash(CMachine* m, void* ret) {
    uint256_t retHash = static_cast<Machine*>(m)->hash();
    std::array<unsigned char, 32> val;
    to_big_endian(retHash, val.begin());
    std::copy(val.begin(), val.end(), reinterpret_cast<char*>(ret));
}

void* machineClone(CMachine* m) {
    Machine* mach = new Machine(*(static_cast<Machine*>(m)));
    return static_cast<void*>(mach);
}

void machinePrint(CMachine* m) {
    Machine* mach = new Machine(*(static_cast<Machine*>(m)));
    std::cout << "Machine info\n" << *mach << std::endl;
}

void machineInboxHash(CMachine* m, void* ret) {
    uint256_t retHash = static_cast<Machine*>(m)->inboxHash();
    std::array<unsigned char, 32> val;
    to_big_endian(retHash, val.begin());
    std::copy(val.begin(), val.end(), reinterpret_cast<char*>(ret));
}

int machineHasPendingMessages(CMachine* m) {
    Machine* mach = static_cast<Machine*>(m);
    return mach->hasPendingMessages();
}

void machineSendOnchainMessage(CMachine* m, void* data) {
    Machine* mach = static_cast<Machine*>(m);
    auto dataPtr = reinterpret_cast<char*>(data);
    auto val = deserialize_value(dataPtr, mach->getPool());
    Message msg;
    auto success = msg.deserialize(val);
    if (!success) {
        throw std::runtime_error("Machine recieved invalid message");
    }
    mach->sendOnchainMessage(msg);
}

void machineDeliverOnchainMessages(CMachine* m) {
    Machine* mach = static_cast<Machine*>(m);
    mach->deliverOnchainMessages();
}

ByteSlice machineMarshallForProof(CMachine* m) {
    Machine* mach = static_cast<Machine*>(m);
    std::vector<unsigned char> buffer;
    auto proof = mach->marshalForProof();
    auto proofData = (unsigned char*)malloc(proof.size());
    std::copy(proof.begin(), proof.end(), proofData);
    auto voidData = reinterpret_cast<void*>(proofData);
    return {voidData, static_cast<int>(proof.size())};
}

void machineSendOffchainMessages(CMachine* m, void* rawData, int messageCount) {
    Machine* mach = static_cast<Machine*>(m);
    std::vector<Message> messages;
    auto data = reinterpret_cast<char*>(rawData);
    for (int i = 0; i < messageCount; i++) {
        auto val = deserialize_value(data, mach->getPool());
        messages.emplace_back();
        auto success = messages.back().deserialize(val);
        if (!success) {
            throw std::runtime_error("Machine recieved invalid message");
        }
    }
    mach->sendOffchainMessages(messages);
}

RawAssertion machineExecuteAssertion(CMachine* m,
                                     uint64_t maxSteps,
                                     uint64_t timeboundStart,
                                     uint64_t timeboundEnd) {
    Machine* mach = static_cast<Machine*>(m);
    Assertion assertion = mach->run(maxSteps, timeboundStart, timeboundEnd);
    std::vector<unsigned char> outMsgData;
    for (const auto& outMsg : assertion.outMessages) {
        marshal_value(outMsg.toValue(mach->getPool()), outMsgData);
    }
    std::vector<unsigned char> logData;
    for (const auto& log : assertion.logs) {
        marshal_value(log, logData);
    }

    unsigned char* cMessageData = (unsigned char*)malloc(outMsgData.size());
    std::copy(outMsgData.begin(), outMsgData.end(), cMessageData);

    unsigned char* cLogData = (unsigned char*)malloc(logData.size());
    std::copy(logData.begin(), logData.end(), cLogData);

    return {cMessageData,
            static_cast<int>(outMsgData.size()),
            static_cast<int>(assertion.outMessages.size()),
            cLogData,
            static_cast<int>(logData.size()),
            static_cast<int>(assertion.logs.size()),
            assertion.stepCount};
}