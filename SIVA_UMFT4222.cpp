/**
 * @file spi_slave_test_no_protocol_slave_side.cpp
 *
 * @author FTDI
 * @date 2016-09-9
 *
 * Copyright © 2011 Future Technology Devices International Limited
 * Company Confidential
 *
 * Rivision History:
 * 1.0 - initial version
 */

 //------------------------------------------------------------------------------
 // This sample must be used with another application spi_slave_test_no_protocol_master_side.cpp
 // It use two FT4222H , one act as spi master , the ohter act as spi slave

 // design ourself protocol to spi slave transmision
 // this provides a simple protocol here, you can do yours protocol.

 // FOR spi slave receiving protocl
 // byte1 : sync word  (0x5A)
 // byte2 : write/read  (write:0 , read : 1)
 // byte3 : size
 // byte 4~? : data          // read command does not have this part.


 // FOR spi slave sending protocl
 // byte1 : sync word  (0x5A)
 // byte2 : size
 // byte3~? : data




#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <conio.h>

//To read file stream
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>

//------------------------------------------------------------------------------
// include FTDI libraries
//
#include "ftd2xx.h"
#include "LibFT4222.h"

class TestPatternGenerator
{
public:
    TestPatternGenerator(uint16 size)
    {
        data.resize(size);

        for (uint16 i = 0; i < data.size(); i++)
        {
            data[i] = (uint8)i;
        }
    }

public:
    std::vector< unsigned char > data;
};

std::vector< FT_DEVICE_LIST_INFO_NODE > g_FTAllDevList;
std::vector< FT_DEVICE_LIST_INFO_NODE > g_FT4222DevList;

#define SYNC_WORD       0x5A
#define CMD_WRITE       0x00
#define CMD_READ        0x01

#define CMD_SIVA_SPI_INIT 0x1A
#define CMD_SIVA_SPI_START 0x08
#define CMD_SIVA_SPI_RDATAC 0x10
#define CMD_SIVA_SPI_RREG 0x20
#define CMD_SIVA_SPI_RREG2 0x21


#define CMD_SIVA_RPLY_SUCCESS 0x73

//------------------------------------------------------------------------------
// Read File *

std::vector<std::string> getNextLineAndSplitIntoTokens(std::istream& str)
{
    std::vector<std::string>   result;
    std::string                line;
    std::getline(str, line);

    std::stringstream          lineStream(line);
    std::string                cell;

    while (std::getline(lineStream, cell, ','))
    {
        result.push_back(cell);
    }
    // This checks for a trailing comma with no data after it.
    if (!lineStream && cell.empty())
    {
        // If there was a trailing comma then add an empty element.
        result.push_back("");
    }
    return result;
}

class CSVRow
{
public:
    std::string_view operator[](std::size_t index) const
    {
        return std::string_view(&m_line[m_data[index] + 1], m_data[index + 1] - (m_data[index] + 1));
    }
    std::size_t size() const
    {
        return m_data.size() - 1;
    }
    void readNextRow(std::istream& str)
    {
        std::getline(str, m_line);

        m_data.clear();
        m_data.emplace_back(-1);
        std::string::size_type pos = 0;
        while ((pos = m_line.find(',', pos)) != std::string::npos)
        {
            m_data.emplace_back(pos);
            ++pos;
        }
        // This checks for a trailing comma with no data after it.
        pos = m_line.size();
        m_data.emplace_back(pos);
    }
private:
    std::string         m_line;
    std::vector<int>    m_data;
};

std::istream& operator>>(std::istream& str, CSVRow& data)
{
    data.readNextRow(str);
    return str;
}
class CSVIterator
{
public:
    typedef std::input_iterator_tag     iterator_category;
    typedef CSVRow                      value_type;
    typedef std::size_t                 difference_type;
    typedef CSVRow* pointer;
    typedef CSVRow& reference;

    CSVIterator(std::istream& str) :m_str(str.good() ? &str : NULL) { ++(*this); }
    CSVIterator() :m_str(NULL) {}

    // Pre Increment
    CSVIterator& operator++() { if (m_str) { if (!((*m_str) >> m_row)) { m_str = NULL; } }return *this; }
    // Post increment
    CSVIterator operator++(int) { CSVIterator    tmp(*this); ++(*this); return tmp; }
    CSVRow const& operator*()   const { return m_row; }
    CSVRow const* operator->()  const { return &m_row; }

    bool operator==(CSVIterator const& rhs) { return ((this == &rhs) || ((this->m_str == NULL) && (rhs.m_str == NULL))); }
    bool operator!=(CSVIterator const& rhs) { return !((*this) == rhs); }
private:
    std::istream* m_str;
    CSVRow              m_row;
};






//------------------------------------------------------------------------------
inline std::string DeviceFlagToString(DWORD flags)
{
    std::string msg;
    msg += (flags & 0x1) ? "DEVICE_OPEN" : "DEVICE_CLOSED";
    msg += ", ";
    msg += (flags & 0x2) ? "High-speed USB" : "Full-speed USB";
    return msg;
}

void ListFtUsbDevices()
{
    FT_STATUS ftStatus = 0;

    DWORD numOfDevices = 0;
    ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

    for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
    {
        FT_DEVICE_LIST_INFO_NODE devInfo;
        memset(&devInfo, 0, sizeof(devInfo));

        ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
            devInfo.SerialNumber,
            devInfo.Description,
            &devInfo.ftHandle);

        if (FT_OK == ftStatus)
        {
            printf("Dev %d:\n", iDev);
            printf("  Flags= 0x%x, (%s)\n", devInfo.Flags, DeviceFlagToString(devInfo.Flags).c_str());
            printf("  Type= 0x%x\n", devInfo.Type);
            printf("  ID= 0x%x\n", devInfo.ID);
            printf("  LocId= 0x%x\n", devInfo.LocId);
            printf("  SerialNumber= %s\n", devInfo.SerialNumber);
            printf("  Description= %s\n", devInfo.Description);
            printf("  ftHandle= 0x%x\n", devInfo.ftHandle);

            const std::string desc = devInfo.Description;
            g_FTAllDevList.push_back(devInfo);

            if (desc == "FT4222" || desc == "FT4222 A")
            {
                g_FT4222DevList.push_back(devInfo);
            }
        }
    }
}

void parse_packet_siva(FT_HANDLE ftHandle, std::vector<unsigned char>& dataBuf, bool* sendSPIRDATAC)
{
    // skip all non "sync word " at the first byte.
    //while (dataBuf.size() > 0)
    //{
    //    if (dataBuf[0] == SYNC_WORD)
    //    {
    //        printf("SIVA: skip all non sync word at the first byte.\n");
    //        break;
    //    }
    //    dataBuf.erase(dataBuf.begin());
    //}

    // data is not enough to parse, wait for another input
    //if (dataBuf.size() < 3)
    //{
    //    printf("SIVA: Data is not enough to parse, wait for another input\n");
    //    return;
    //}

    switch (dataBuf[0])
    {
    case CMD_SIVA_SPI_INIT:
    {
        printf("SIVA SPI INIT\n");
        dataBuf.erase(dataBuf.begin());
    }
    break;
    case CMD_SIVA_SPI_START:
    {
        printf("SIVA SPI START\n");
        dataBuf.erase(dataBuf.begin());
    }
    break;
    case CMD_SIVA_SPI_RDATAC:
    {
        printf("SIVA SPI RDATAC\n");
        dataBuf[0] = 0x5B;
        *sendSPIRDATAC = true;
    }
    break;
    case CMD_SIVA_SPI_RREG:
    {
        uint16 sizeTransferred;
        uint8 size = 1; // dataBuf[2] - 1;
        std::vector<unsigned char> sendData;
        sendData.resize(size + 1); // including sync word and size
        sendData[0] = SYNC_WORD;

        std::vector<unsigned char> testPattern;
        testPattern.resize(size);
        testPattern[0] = 0x08;

        // all data are sequential , we can check data at spi slave
        memcpy(&sendData[1], &testPattern[0], size);

        printf("get a read request , read size = %d\n", size);

        FT4222_SPISlave_Write(ftHandle, &sendData[0], sendData.size(), &sizeTransferred);

        // drop the used data
        dataBuf.erase(dataBuf.begin(), dataBuf.begin() + 3);
        printf("=============================================\n", size);

    }
    break;
    //case CMD_WRITE:
    //{
    //    // check data length
    //    uint16 size = dataBuf[2];
    //    // wait all data write from spi master
    //    if (dataBuf.size() < (size + 3))
    //    {
    //        //  printf("we do not get enough data\n");
    //        return;
    //    }
    //    TestPatternGenerator testPattern(size);

    //    printf("get a write request , write size = %d\n", size);

    //    // check if all data are correct
    //    if (memcmp(&dataBuf[3], &testPattern.data[0], size) == 0)
    //    {
    //        printf("[OK]    [SPI master ==> SPI slave]Read data from Master and all data are equivalent\n");
    //    }
    //    else
    //    {
    //        printf("[ERROR] [SPI master ==> SPI slave]Data are not equivalent\n");
    //    }

    //    dataBuf.erase(dataBuf.begin(), dataBuf.begin() + 3 + size);
    //    printf("=============================================\n", size);
    //}
    //break;
    default:
    {
        if (*sendSPIRDATAC == true) {
            uint16 sizeTransferred;
            uint8 size = 6; // dataBuf[2] - 1;
            std::vector<unsigned char> sendData;
            sendData.resize(size + 1); // including sync word and size
            sendData[0] = SYNC_WORD;

            std::vector<unsigned char> testPattern;
            testPattern.resize(size);
            testPattern[0] = 0x01;
            testPattern[1] = 0x02;
            testPattern[2] = 0x03;
            testPattern[3] = 0x04;
            testPattern[4] = 0x05;
            testPattern[5] = 0x06;
            //testPattern[6] = 0x07;

            // all data are sequential , we can check data at spi slave
            memcpy(&sendData[1], &testPattern[0], size);

            printf("get a read request , read size = %d\n", size);

            FT4222_SPISlave_Write(ftHandle, &sendData[0], sendData.size(), &sizeTransferred);
        }
        else {
            printf("data error\n");
            return;
        }
    }
    break;

    }

}


void FT4222_SPISlave_Get(FT_HANDLE ftHandle, uint16 rxSize, uint16 sizeTransferred, std::vector<unsigned char> recvBuf, std::vector<unsigned char> dataBuf) {

}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
int main(int argc, char const* argv[])
{

    std::ifstream file("rr100_MITBIH.csv");
    printf("File loaded");
    for (CSVIterator loop(file); loop != CSVIterator(); ++loop)
    {
        printf("file : %d\n", *loop)[2]);
        //std::cout << "3rd Element(" << (*loop)[2] << ")\n";
    }

    //ListFtUsbDevices();

    //if (g_FT4222DevList.empty()) {
    //    printf("No FT4222 device is found!\n");
    //    return 0;
    //}
    //int num = 0;
    //for (int idx = 0; idx < 10; idx++)
    //{
    //    printf("select dev num(0~%d) as spi slave\n", g_FTAllDevList.size() - 1);
    //    //num = getch();
    //    //num = num - '0';
    //    if (num >= 0 && num < g_FTAllDevList.size())
    //    {
    //        break;
    //    }
    //    else
    //    {
    //        printf("input error , please input again\n");
    //    }

    //}

    //FT_HANDLE ftHandle = NULL;
    //FT_STATUS ftStatus;
    //FT_STATUS ft4222_status;
    //ftStatus = FT_Open(num, &ftHandle);
    //if (FT_OK != ftStatus)
    //{
    //    printf("Open a FT4222 device failed!\n");
    //    return 0;
    //}

    //printf("\n\n");
    //printf("Init FT4222 as SPI slave\n");

    //FT_HANDLE ftHandle2 = NULL;
    //FT_STATUS ftStatus2;
    //ftStatus2 = FT_OpenEx("FT4222 B", FT_OPEN_BY_DESCRIPTION, &ftHandle2);
    ////ftStatus2 = FT_OpenEx((PVOID)g_FT4222DevList[1].LocId, FT_OPEN_BY_LOCATION, &ftHandle2);
    //if (FT_OK != ftStatus2)
    //{
    //    printf("Open a FT4222 device as GPIO failed!\n");
    //    return 0;
    //}
    //printf("\n");
    //printf("Init FT4222 as GPIO\n");


    //ftStatus = FT4222_SetClock(ftHandle, SYS_CLK_80);
    //if (FT_OK != ftStatus)
    //{
    //    printf("Set FT4222 clock to 80MHz failed!\n");
    //    return 0;
    //}


    //ftStatus = FT4222_SPISlave_InitEx(ftHandle, SPI_SLAVE_NO_PROTOCOL);
    //if (FT_OK != ftStatus)
    //{
    //    printf("Init FT4222 as SPI slave device failed!\n");
    //    return 0;
    //}

    //// set spi cpol and cpha to mode 3
    //if (FT4222_OK != FT4222_SPISlave_SetMode(ftHandle, CLK_IDLE_LOW, CLK_TRAILING))
    //{
    //    printf("FT4222 set SPI mode failed!\n"); // set spi mode failed
    //    return 0;
    //}

    //ftStatus = FT4222_SPI_SetDrivingStrength(ftHandle, DS_4MA, DS_4MA, DS_4MA);
    //if (FT_OK != ftStatus)
    //{
    //    printf("Set SPI Slave driving strength failed!\n");
    //    return 0;
    //}

    //ftStatus = FT_SetUSBParameters(ftHandle, 4 * 1024, 0);
    //if (FT_OK != ftStatus)
    //{
    //    printf("FT_SetUSBParameters failed!\n");
    //    return 0;
    //}

    //// disable Suspenout, it use the same pin with gpio2
    //FT4222_SetSuspendOut(ftHandle2, false);
    ////disable interrupt , enable gpio 3
    //FT4222_SetWakeUpInterrupt(ftHandle2, false);

    //GPIO_Dir gpioDir[4];

    //gpioDir[0] = GPIO_OUTPUT;
    //gpioDir[1] = GPIO_OUTPUT;
    //gpioDir[2] = GPIO_OUTPUT;
    //gpioDir[3] = GPIO_OUTPUT;

    //// master initialize
    //FT4222_GPIO_Init(ftHandle2, gpioDir);    

    //bool toggle = true;
    ////    toggle gpio2 , the led light will flash
    //FT4222_GPIO_Write(ftHandle2, GPIO_PORT2, 1);

    ////    Start to work as SPI slave, and read/write data
    //uint16 sizeTransferred = 0;
    //uint16 rxSize;
    //std::vector<unsigned char> recvBuf;
    //std::vector<unsigned char> dataBuf; // data need to be parsed
    ////char * dataBufC= "NORDIC";
    //bool sendSPIRDATAC = false;


    //

    //while (1)
    //{

    //    if (sendSPIRDATAC == true) {
    //            Sleep(4);
    //            if (toggle) {
    //                if (FT_OK != FT4222_GPIO_Write(ftHandle2, GPIO_PORT2, 0))
    //                {
    //                    printf("1 gpio failed!\n");
    //                    return 0;
    //                }
    //                else {
    //                    //FT4222_SPISlave_Get(ftHandle,rxSize, sizeTransferred, recvBuf, dataBuf);
    //                    if (FT4222_SPISlave_GetRxStatus(ftHandle, &rxSize) == FT_OK)
    //                    {
    //                        if (rxSize > 0)
    //                        {
    //                            recvBuf.resize(rxSize);
    //                            if (FT4222_SPISlave_Read(ftHandle, &recvBuf[0], rxSize, &sizeTransferred) == FT_OK)
    //                            {
    //                                if (sizeTransferred != rxSize)
    //                                {
    //                                    printf("read data size is not equal\n");
    //                                }
    //                                else
    //                                {
    //                                    // append data to dataBuf
    //                                    if (!sendSPIRDATAC)
    //                                        dataBuf.insert(dataBuf.end(), recvBuf.begin(), recvBuf.end());


    //                                }
    //                            }
    //                            else
    //                            {
    //                                printf("FT4222_SPISlave_Read error\n");
    //                            }
    //                        }


    //                        if (dataBuf.size() > 0) {
    //                            for (int jj = 0; jj < dataBuf.size(); jj++) {
    //                                printf("%d,", dataBuf[jj]);
    //                            }
    //                            printf("dataBuf.size %u\n", dataBuf.size());
    //                            parse_packet_siva(ftHandle, dataBuf, &sendSPIRDATAC);


    //                            //dataBuf.erase(dataBuf.begin(), dataBuf.begin() + dataBuf.size());

    //                        }
    //                    }
    //                    printf("gpio request rxSize: %d!\n", rxSize);
    //                    toggle = !toggle;
    //                }
    //                //Sleep(5);
    //                if (FT_OK != FT4222_GPIO_Write(ftHandle2, GPIO_PORT2, 1)) {
    //                    printf("2 gpio failed!\n");
    //                    return 0;
    //                }
    //                //sendSPIRDATAC = false;
    //                //dataBuf.erase(dataBuf.begin()+1, dataBuf.begin() + dataBuf.size());
    //            }
    //            else {
    //                if (FT_OK != FT4222_GPIO_Write(ftHandle2, GPIO_PORT2, 1)) {
    //                    printf("3 gpio failed!\n");
    //                    return 0;
    //                }
    //                toggle = !toggle;
    //            }
    //            //parse_packet_siva(ftHandle, dataBuf);    
    //    }
    //    else {
    //        //FT4222_SPISlave_Get(ftHandle, rxSize, sizeTransferred, recvBuf, dataBuf);
    //        if (FT4222_SPISlave_GetRxStatus(ftHandle, &rxSize) == FT_OK)
    //        {
    //            if (rxSize > 0)
    //            {
    //                recvBuf.resize(rxSize);
    //                if (FT4222_SPISlave_Read(ftHandle, &recvBuf[0], rxSize, &sizeTransferred) == FT_OK)
    //                {
    //                    if (sizeTransferred != rxSize)
    //                    {
    //                        printf("read data size is not equal\n");
    //                    }
    //                    else
    //                    {
    //                        // append data to dataBuf
    //                        if (!sendSPIRDATAC)
    //                            dataBuf.insert(dataBuf.end(), recvBuf.begin(), recvBuf.end());


    //                    }
    //                }
    //                else
    //                {
    //                    printf("FT4222_SPISlave_Read error\n");
    //                }
    //            }


    //            if (dataBuf.size() > 0) {
    //                for (int jj = 0; jj < dataBuf.size(); jj++) {
    //                    printf("%d,", dataBuf[jj]);
    //                }
    //                printf("dataBuf.size %u\n", dataBuf.size());
    //                parse_packet_siva(ftHandle, dataBuf, &sendSPIRDATAC);


    //                //dataBuf.erase(dataBuf.begin(), dataBuf.begin() + dataBuf.size());

    //            }
    //        }
    //    }

    //    
    //    
    //    
    //    //parse_packet_siva(ftHandle);
    //    //if (sendSPIRDATAC == true) {
    //    //    Sleep(100);
    //    //    if (toggle) {
    //    //        if (FT_OK != FT4222_GPIO_Write(ftHandle, GPIO_PORT2, 0))
    //    //        {
    //    //            printf("gpio failed!\n");
    //    //            return 0;
    //    //        }
    //    //        else toggle = !toggle;
    //    //        Sleep(5);
    //    //        FT4222_GPIO_Write(ftHandle, GPIO_PORT2, 1);
    //    //        //sendSPIRDATAC = false;
    //    //        //dataBuf.erase(dataBuf.begin()+1, dataBuf.begin() + dataBuf.size());
    //    //    }
    //    //    else {
    //    //        FT4222_GPIO_Write(ftHandle, GPIO_PORT2, 1);
    //    //        toggle = !toggle;
    //    //    }
    //    //    //parse_packet_siva(ftHandle, dataBuf);    
    //    //}
    //}

    //printf("UnInitialize FT4222\n");
    //FT4222_UnInitialize(ftHandle);
    //printf("UnInitialize FT4222\n");
    //FT4222_UnInitialize(ftHandle2);

    //printf("Close FT device\n");
    //FT_Close(ftHandle);
    //printf("Close FT device\n");
    //FT_Close(ftHandle2);

    return 0;
}



/**
 * @file getting_started.cpp
 *
 * @author FTDI
 * @date 2014-07-01
 *
 * Copyright © 2011 Future Technology Devices International Limited
 * Company Confidential
 *
 * The sample source code is provided as an example and is neither guaranteed
 * nor supported by FTDI.
 *
 * Rivision History:
 * 1.0 - initial version
 */


 /*
 //------------------------------------------------------------------------------
 #include <windows.h>

 #include <stdio.h>
 #include <stdlib.h>
 #include <vector>
 #include <string>

 //------------------------------------------------------------------------------
 // include FTDI libraries
 //
 #include "ftd2xx.h"
 #include "LibFT4222.h"


 std::vector< FT_DEVICE_LIST_INFO_NODE > g_FT4222DevList;

 //------------------------------------------------------------------------------
 inline std::string DeviceFlagToString(DWORD flags)
 {
     std::string msg;
     msg += (flags & 0x1)? "DEVICE_OPEN" : "DEVICE_CLOSED";
     msg += ", ";
     msg += (flags & 0x2)? "High-speed USB" : "Full-speed USB";
     return msg;
 }

 void ListFtUsbDevices()
 {
     FT_STATUS ftStatus = 0;

     DWORD numOfDevices = 0;
     ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

     for(DWORD iDev=0; iDev<numOfDevices; ++iDev)
     {
         FT_DEVICE_LIST_INFO_NODE devInfo;
         memset(&devInfo, 0, sizeof(devInfo));

         ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
                                         devInfo.SerialNumber,
                                         devInfo.Description,
                                         &devInfo.ftHandle);

         if (FT_OK == ftStatus)
         {
             printf("Dev %d:\n", iDev);
             printf("  Flags= 0x%x, (%s)\n", devInfo.Flags, DeviceFlagToString(devInfo.Flags).c_str());
             printf("  Type= 0x%x\n",        devInfo.Type);
             printf("  ID= 0x%x\n",          devInfo.ID);
             printf("  LocId= 0x%x\n",       devInfo.LocId);
             printf("  SerialNumber= %s\n",  devInfo.SerialNumber);
             printf("  Description= %s\n",   devInfo.Description);
             printf("  ftHandle= 0x%x\n",    devInfo.ftHandle);

             const std::string desc = devInfo.Description;
             if(desc == "FT4222" || desc == "FT4222 A")
             {
                 g_FT4222DevList.push_back(devInfo);
             }
         }
     }
 }

 //------------------------------------------------------------------------------
 // main
 //------------------------------------------------------------------------------
 int main(int argc, char const *argv[])
 {
     ListFtUsbDevices();

     if(g_FT4222DevList.empty()) {
         printf("No FT4222 device is found!\n");
         return 0;
     }

     FT_HANDLE ftHandle = NULL;

     FT_STATUS ftStatus;
     ftStatus = FT_OpenEx((PVOID)g_FT4222DevList[0].LocId, FT_OPEN_BY_LOCATION, &ftHandle);
     if (FT_OK != ftStatus)
     {
         printf("Open a FT4222 device failed!\n");
         return 0;
     }

     printf("\n\n");
     printf("Init FT4222 as SPI master\n");
     ftStatus = FT4222_SPIMaster_Init(ftHandle, SPI_IO_SINGLE, CLK_DIV_4, CLK_IDLE_LOW, CLK_LEADING, 0x01);
     if (FT_OK != ftStatus)
     {
         printf("Init FT4222 as SPI master device failed!\n");
         return 0;
     }

     // TODO:
     //    Start to work as SPI master, and read/write data to a SPI slave
     //    FT4222_SPIMaster_SingleWrite
     //    FT4222_SPIMaster_SingleRead
     //    FT4222_SPIMaster_SingleReadWrite
     //ftStatus = FT4222_SPIMaster_SingleRead(ftHandle,)
     uint8 recvData[10];
     uint16 sizeTransferred;
     ftStatus = FT4222_SPIMaster_SingleRead(ftHandle, &recvData[0], 10, &sizeTransferred,
         true);
     if (FT4222_OK != ftStatus)
     {
         // spi master read failed
         return 0;
     }
     else {

         printf("%u,", recvData[0]);
     }

     printf("TODO ...\n");
     printf("\n");



     printf("UnInitialize FT4222\n");
     FT4222_UnInitialize(ftHandle);

     printf("Close FT device\n");
     FT_Close(ftHandle);
     return 0;
 }
 */