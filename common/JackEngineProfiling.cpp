/*
Copyright (C) 2008 Grame & RTL

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software 
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "JackEngineProfiling.h"
#include "JackGraphManager.h"
#include "JackClientControl.h"
#include "JackEngineControl.h"
#include "JackClientInterface.h"
#include "JackGlobals.h"
#include "JackTime.h"

#include <iostream>
#include <fstream>

namespace Jack
{

JackEngineProfiling::JackEngineProfiling():fAudioCycle(0),fMeasuredClient(0)
{
    jack_info("Engine profiling activated, beware %ld MBytes are needed to record profiling points...", sizeof(fProfileTable) / (1024 * 1024));
    
    // Force memory page in
    memset(fProfileTable, 0, sizeof(fProfileTable));
}

JackEngineProfiling::~JackEngineProfiling()
{
    std::ofstream fStream("JackEngineProfiling.log", std::ios_base::ate);
    jack_info("Write server and clients timing data...");

    if (!fStream.is_open()) {
        jack_error("JackEngineProfiling::Save cannot open JackEngineProfiling.log file");
    } else {
    
        // For each measured point
        for (int i = 2; i < TIME_POINTS; i++) {
            
            // Driver timing values
            long d1 = long(fProfileTable[i].fCurCycleBegin - fProfileTable[i - 1].fCurCycleBegin);
            long d2 = long(fProfileTable[i].fPrevCycleEnd - fProfileTable[i - 1].fCurCycleBegin);
            
            if (d1 <= 0 || fProfileTable[i].fAudioCycle <= 0)
                continue; // Skip non valid cycles
                
            // Print driver delta and end cycle
            fStream << d1 << "\t" << d2 << "\t";
             
            // For each measured client
            for (unsigned int j = 0; j < fMeasuredClient; j++) { 
            
                int ref = fIntervalTable[j].fRefNum;
            
                // Is valid client cycle 
                 if (fProfileTable[i].fClientTable[ref].fStatus != NotTriggered) {
             
                    long d5 = long(fProfileTable[i].fClientTable[ref].fSignaledAt - fProfileTable[i - 1].fCurCycleBegin);
                    long d6 = long(fProfileTable[i].fClientTable[ref].fAwakeAt - fProfileTable[i - 1].fCurCycleBegin);
                    long d7 = long(fProfileTable[i].fClientTable[ref].fFinishedAt - fProfileTable[i - 1].fCurCycleBegin);
                        
                     fStream << ref << "\t" ;
                     fStream << ((d5 > 0) ? d5 : 0) << "\t";
                     fStream << ((d6 > 0) ? d6 : 0) << "\t" ;
                     fStream << ((d7 > 0) ? d7 : 0) << "\t";
                     fStream << ((d6 > 0 && d5 > 0) ? (d6 - d5) : 0) << "\t" ;
                     fStream << ((d7 > 0 && d6 > 0) ? (d7 - d6) : 0) << "\t" ;
                     fStream << fProfileTable[i].fClientTable[ref].fStatus << "\t" ;;
                
                 } else { // Print tabs
                     fStream <<  "\t  \t  \t  \t  \t  \t \t";
                }
            }
            
            // Terminate line
            fStream << std::endl;
        }
    }
    
    // Driver period
     std::ofstream fStream1("Timing1.plot", std::ios_base::ate);
 
    if (!fStream1.is_open()) {
        jack_error("JackEngineProfiling::Save cannot open Timing1.log file");
    } else {
        
        fStream1 << "set grid\n";
        fStream1 <<  "set title \"Audio driver timing\"\n";
        fStream1 <<  "set xlabel \"audio cycles\"\n";
        fStream1 <<  "set ylabel \"usec\"\n";
        fStream1 <<  "plot \"JackEngineProfiling.log\" using 1 title \"Audio period\" with lines \n";
        
        fStream1 <<  "set output 'Timing1.pdf\n";
        fStream1 <<  "set terminal pdf\n";
        
        fStream1 <<  "set grid\n";
        fStream1 <<  "set title \"Audio driver timing\"\n";
        fStream1 <<  "set xlabel \"audio cycles\"\n";
        fStream1 <<  "set ylabel \"usec\"\n";
        fStream1 <<  "plot \"JackEngineProfiling.log\" using 1 title \"Audio period\" with lines \n";
    }
    
    // Driver end date
    std::ofstream fStream2("Timing2.plot", std::ios_base::ate);
  
    if (!fStream2.is_open()) {
        jack_error("JackEngineProfiling::Save cannot open Timing2.log file");
    } else {
   
        fStream2 << "set grid\n";
        fStream2 <<  "set title \"Driver end date\"\n";
        fStream2 <<  "set xlabel \"audio cycles\"\n";
        fStream2 <<  "set ylabel \"usec\"\n";
        fStream2 <<  "plot  \"JackEngineProfiling.log\" using 2 title \"Driver end date\" with lines \n";
    
        fStream2 <<  "set output 'Timing2.pdf\n";
        fStream2 <<  "set terminal pdf\n";
    
        fStream2 <<  "set grid\n";
        fStream2 <<  "set title \"Driver end date\"\n";
        fStream2 <<  "set xlabel \"audio cycles\"\n";
        fStream2 <<  "set ylabel \"usec\"\n";
        fStream2 <<  "plot  \"JackEngineProfiling.log\" using 2 title \"Driver end date\" with lines \n";
    }
        
    // Clients end date
    if (fMeasuredClient > 0) {
        std::ofstream fStream3("Timing3.plot", std::ios_base::ate);
        
        if (!fStream3.is_open()) {
            jack_error("JackEngineProfiling::Save cannot open Timing3.log file");
        } else {
        
            fStream3 << "set multiplot\n";
            fStream3 << "set grid\n";
            fStream3 << "set title \"Clients end date\"\n";
            fStream3 << "set xlabel \"audio cycles\"\n";
            fStream3 << "set ylabel \"usec\"\n";
            fStream3 << "plot ";
            for (unsigned int i = 0; i < fMeasuredClient; i++) {
                if (i == 0) {
                    if (i + 1 == fMeasuredClient) { // Last client
                        fStream3 << "\"JackEngineProfiling.log\" using 1 title \"Audio period\" with lines,\"JackEngineProfiling.log\" using ";
                        fStream3 <<  ((i + 1) * 7) - 1;
                        fStream3 << " title \"" << fIntervalTable[i].fName << "\"with lines";
                     } else {
                        fStream3 << "\"JackEngineProfiling.log\" using 1 title \"Audio period\" with lines,\"JackEngineProfiling.log\" using ";
                        fStream3 <<  ((i + 1) * 7) - 1;
                        fStream3 << " title \"" << fIntervalTable[i].fName << "\"with lines,";
                    }
                } else if (i + 1 == fMeasuredClient) { // Last client
                    fStream3 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) - 1  << " title \"" << fIntervalTable[i].fName << "\" with lines";
                } else {
                    fStream3 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) - 1  << " title \"" << fIntervalTable[i].fName << "\" with lines,";
                }
            }
        
            fStream3 << "\n unset multiplot\n";  
            fStream3 << "set output 'Timing3.pdf\n";
            fStream3 << "set terminal pdf\n";
        
            fStream3 << "set multiplot\n";
            fStream3 << "set grid\n";
            fStream3 << "set title \"Clients end date\"\n";
            fStream3 << "set xlabel \"audio cycles\"\n";
            fStream3 << "set ylabel \"usec\"\n";
            fStream3 << "plot ";
            for (unsigned int i = 0; i < fMeasuredClient; i++) {
                if (i == 0) {
                    if ((i + 1) == fMeasuredClient) { // Last client
                        fStream3 << "\"JackEngineProfiling.log\" using 1 title \"Audio period\" with lines,\"JackEngineProfiling.log\" using ";
                        fStream3 <<  ((i + 1) * 7) - 1;
                        fStream3 << " title \"" << fIntervalTable[i].fName << "\"with lines";
                    } else {
                        fStream3 << "\"JackEngineProfiling.log\" using 1 title \"Audio period\" with lines,\"JackEngineProfiling.log\" using ";
                        fStream3 <<  ((i + 1) * 7) - 1;
                        fStream3 << " title \"" << fIntervalTable[i].fName << "\"with lines,";
                    }
                } else if ((i + 1) == fMeasuredClient) { // Last client
                    fStream3 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) - 1  << " title \"" << fIntervalTable[i].fName << "\" with lines";
                } else {
                    fStream3 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) - 1  << " title \"" << fIntervalTable[i].fName << "\" with lines,";
                }
            }
        }
    }

    // Clients scheduling
    if (fMeasuredClient > 0) {
        std::ofstream fStream4("Timing4.plot", std::ios_base::ate);
        
        if (!fStream4.is_open()) {
            jack_error("JackEngineProfiling::Save cannot open Timing4.log file");
        } else {
        
            fStream4 << "set multiplot\n";
            fStream4 << "set grid\n";
            fStream4 << "set title \"Clients scheduling latency\"\n";
            fStream4 << "set xlabel \"audio cycles\"\n";
            fStream4 << "set ylabel \"usec\"\n";
            fStream4 << "plot ";
            for (unsigned int i = 0; i < fMeasuredClient; i++) {
                if ((i + 1) == fMeasuredClient) { // Last client
                    fStream4 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7)  << " title \"" << fIntervalTable[i].fName << "\" with lines";
                 } else {
                     fStream4 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7)  << " title \"" << fIntervalTable[i].fName << "\" with lines,";
                }
            }
            
            fStream4 << "\n unset multiplot\n";  
            fStream4 << "set output 'Timing4.pdf\n";
            fStream4 << "set terminal pdf\n";
            
            fStream4 << "set multiplot\n";
            fStream4 << "set grid\n";
            fStream4 << "set title \"Clients scheduling latency\"\n";
            fStream4 << "set xlabel \"audio cycles\"\n";
            fStream4 << "set ylabel \"usec\"\n";
            fStream4 << "plot ";
            for (unsigned int i = 0; i < fMeasuredClient; i++) {
                if ((i + 1) == fMeasuredClient) { // Last client
                    fStream4 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7)  << " title \"" << fIntervalTable[i].fName << "\" with lines";
                } else {
                     fStream4 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7)  << " title \"" << fIntervalTable[i].fName << "\" with lines,";
                }
            }
        }
    }
    
     // Clients duration
    if (fMeasuredClient > 0) {
        std::ofstream fStream5("Timing5.plot", std::ios_base::ate);

        if (!fStream5.is_open()) {
            jack_error("JackEngineProfiling::Save cannot open Timing5.log file");
        } else {
        
            fStream5 << "set multiplot\n";
            fStream5 << "set grid\n";
            fStream5 << "set title \"Clients duration\"\n";
            fStream5 << "set xlabel \"audio cycles\"\n";
            fStream5 << "set ylabel \"usec\"\n";
            fStream5 << "plot ";
            for (unsigned int i = 0; i < fMeasuredClient; i++) {
                if ((i + 1) == fMeasuredClient) { // Last client
                    fStream5 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) + 1  << " title \"" << fIntervalTable[i].fName << "\" with lines";
                } else {
                    fStream5 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) + 1  << " title \"" << fIntervalTable[i].fName << "\" with lines,";
                }
            }
            
            fStream5 << "\n unset multiplot\n";  
            fStream5 << "set output 'Timing5.pdf\n";
            fStream5 << "set terminal pdf\n";
            
            fStream5 << "set multiplot\n";
            fStream5 << "set grid\n";
            fStream5 << "set title \"Clients duration\"\n";
            fStream5 << "set xlabel \"audio cycles\"\n";
            fStream5 << "set ylabel \"usec\"\n";
            fStream5 << "plot ";
            for (unsigned int i = 0; i < fMeasuredClient; i++) {
                if ((i + 1) == fMeasuredClient) {// Last client
                    fStream5 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) + 1  << " title \"" << fIntervalTable[i].fName << "\" with lines";
                } else {
                    fStream5 << "\"JackEngineProfiling.log\" using " << ((i + 1) * 7) + 1  << " title \"" << fIntervalTable[i].fName << "\" with lines,";
                }
            }
        }
    }
}

bool JackEngineProfiling::CheckClient(const char* name, int cur_point)
{
    for (int i = 0; i < MEASURED_CLIENTS; i++) {
       if (strcmp(fIntervalTable[i].fName, name) == 0) {
            fIntervalTable[i].fEndInterval = cur_point;
            return true;
        }
    }
    return false;
}

void JackEngineProfiling::Profile(JackClientInterface** table, 
                                   JackGraphManager* manager, 
                                   jack_time_t period_usecs,
                                   jack_time_t cur_cycle_begin, 
                                   jack_time_t prev_cycle_end)
{
    fAudioCycle = (fAudioCycle + 1) % TIME_POINTS;
  
    // Keeps cycle data
    fProfileTable[fAudioCycle].fPeriodUsecs = period_usecs;
    fProfileTable[fAudioCycle].fCurCycleBegin = cur_cycle_begin;
    fProfileTable[fAudioCycle].fPrevCycleEnd = prev_cycle_end;
    fProfileTable[fAudioCycle].fAudioCycle = fAudioCycle;

    for (int i = GetEngineControl()->fDriverNum; i < CLIENT_NUM; i++) {
        JackClientInterface* client = table[i];
        JackClientTiming* timing = manager->GetClientTiming(i);
        if (client && client->GetClientControl()->fActive && client->GetClientControl()->fCallback[kRealTimeCallback]) {
           
            if (!CheckClient(client->GetClientControl()->fName, fAudioCycle)) {
                // Keep new measured client
                fIntervalTable[fMeasuredClient].fRefNum = i;
                strcpy(fIntervalTable[fMeasuredClient].fName, client->GetClientControl()->fName);
                fIntervalTable[fMeasuredClient].fBeginInterval = fAudioCycle;
                fIntervalTable[fMeasuredClient].fEndInterval = fAudioCycle;
                fMeasuredClient++;
            }
            fProfileTable[fAudioCycle].fClientTable[i].fRefNum = i;
            fProfileTable[fAudioCycle].fClientTable[i].fSignaledAt = timing->fSignaledAt;
            fProfileTable[fAudioCycle].fClientTable[i].fAwakeAt = timing->fAwakeAt;
            fProfileTable[fAudioCycle].fClientTable[i].fFinishedAt = timing->fFinishedAt;
            fProfileTable[fAudioCycle].fClientTable[i].fStatus = timing->fStatus;
        }
    }
}

JackTimingMeasure* JackEngineProfiling::GetCurMeasure()
{
    return &fProfileTable[fAudioCycle];
}
    
} // end of namespace
