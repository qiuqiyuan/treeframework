#include "LocalOctTree.h"
#include "Common/Options.h"
#include "Algorithm.h"
#include <algorithm>
#include <time.h> 
#include <iostream>
#include "Point.h"
#include "Cell.h"
using namespace std;

const long NOT_INIT = -1;

int LocalOctTree::computeProcID(const Point& p) const
{
    return p.getCode() % (unsigned) opt::numProc;
}

void LocalOctTree::handle(int, const SeqAddMessage& message)
{
    assert(isLocal(message.m_point));
    //need to use the local structure to put content in message to local m_data
    m_data.push_back(message.m_point);
}

bool LocalOctTree::isLocal(const Point& p) const
{
    return computeProcID(p) == opt::rank;
}

void LocalOctTree::add(const Point &p)
{
    if(isLocal(p)){
        //cout << p.x << " "  << p.y << " "  << p.z << " " 
            //<< p.mass << " local --> " <<  computeProcID(p) << endl;
        m_data.push_back(p);
    } else{
        //cout << p.x << " "  << p.y << " "  << p.z << " " 
            //<<  p.mass << " not local --> " << computeProcID(p) << endl;
        m_comm.sendSeqAddMessage(computeProcID(p), p);
    }
}

void LocalOctTree::parseControlMessage(int source)
{
    ControlMessage controlMsg = m_comm.receiveControlMessage();
    switch(controlMsg.msgType)
    {
        case APC_SET_STATE:
            SetState(NetworkActionState(controlMsg.argument));
            break;
        case APC_CHECKPOINT:
            //logger(4) << "checkpoint from " << source << ": "
            //<< controlMsg.argument << '\n';
            m_numReachedCheckpoint++;
            m_checkpointSum += controlMsg.argument;
            break;
        case APC_WAIT:
            SetState(NAS_WAITING);
            m_comm.barrier();
            break;
        case APC_BARRIER:
            assert(m_state == NAS_WAITING);
            m_comm.barrier();
            break;
    }
}


size_t LocalOctTree::pumpNetwork()
{
    for( size_t count = 0; ;count++){
        int senderID;
        APMessage msg = m_comm.checkMessage(senderID);
        switch(msg)
        {
            case APM_CONTROL:
                {
                    parseControlMessage(senderID);
                    //deal with control message before 
                    //any other type of message
                    return ++count;
                }
            case APM_BUFFERED:
                {
                    //Deal all message here until done
                    MessagePtrVector msgs;
                    m_comm.receiveBufferedMessage(msgs);
                    for(MessagePtrVector::iterator 
                            iter = msgs.begin();
                            iter != msgs.end(); iter++){
                        //handle message based on its type
                        (*iter)->handle(senderID, *this);
                        delete (*iter);
                        *iter = 0;
                    }
                    break;
                }
            case APM_NONE:
                return count;
        }
    }
}
void LocalOctTree::EndState()
{
    m_comm.flush();
}

//
// Set the action state 
//
void LocalOctTree::SetState(
        NetworkActionState newState)
{
    //logger(2) << "SetState " << newState
    //<< " (was " << m_state << ")\n";

    // Ensure there are no pending messages
    assert(m_comm.transmitBufferEmpty());

    m_state = newState;

    // Reset the checkpoint counter
    m_numReachedCheckpoint = 0;
    m_checkpointSum = 0;
}

void LocalOctTree::loadPoints()
{
    //Timer timer("LoadSequences");
    if(opt::rank == 0)
        FMMAlgorithms::loadPoints(this, opt::inFile); 
}


//Find local min and max for each dimension
//Find global min and max through MPI ALLreduce
//distribute the result
void LocalOctTree::setUpGlobalMinMax()
{
    //TODO: what do we do with a processor that does not 
    //have any data?
    assert(m_data.size() > 0);
    Point cntPoint = m_data[0];
    double minX(cntPoint.x), maxX(cntPoint.x);
    double minY(cntPoint.y), maxY(cntPoint.y);
    double minZ(cntPoint.z), maxZ(cntPoint.z);

    for(unsigned i=1;i<m_data.size();i++){
        Point cntPoint = m_data[i];
        //For current smallest cord
        if(cntPoint.x < minX){
            minX = cntPoint.x;
        }
        if(cntPoint.y < minY){
            minY = cntPoint.y;
        }
        if(cntPoint.z < minZ){
            minZ = cntPoint.z;
        }

        //For current lareges cord
        if(cntPoint.x > maxX){
            maxX = cntPoint.x;
        }
        if(cntPoint.y > maxY){
            maxY = cntPoint.y;
        }
        if(cntPoint.z > maxZ){
            maxZ = cntPoint.z;
        }
    }
    
    localMinX = minX;
    localMinY = minY;
    localMinZ = minZ;
    localMaxX = maxX;
    localMaxY = maxY;
    localMaxZ = maxZ;

    //set globla dimension
    globalMinX = m_comm.reduce(minX, MIN);
    globalMinY = m_comm.reduce(minY, MIN);
    globalMinZ = m_comm.reduce(minZ, MIN);
    //set global dimension
    globalMaxX = m_comm.reduce(maxX, MAX);
    globalMaxY = m_comm.reduce(maxY, MAX);
    globalMaxZ = m_comm.reduce(maxZ, MAX);
}

void LocalOctTree::printGlobalMinMax()
{
    cout << "DEBUG: globalMinX " << globalMinX <<endl;
    cout << "DEBUG: globalMinY " << globalMinY <<endl;
    cout << "DEBUG: globalMinZ " << globalMinZ <<endl;
    cout << "DEBUG: globalMaxX " << globalMaxX <<endl;
    cout << "DEBUG: globalMaxY " << globalMaxY <<endl;
    cout << "DEBUG: globalMaxZ " << globalMaxZ <<endl;
}


void LocalOctTree::printPoints()
{
    //simple MPI critical section
    int rank = 0;
    while(rank < opt::numProc){
        if(opt::rank == rank){
            if(m_data.size() == 0) {
                cout << "DEBUG " << opt::rank << ": empty" <<endl;
            } else{
                for(unsigned i=0; i<m_data.size();i++) {
                    Point p = m_data[i];
                    cout << "DEBUG " << opt::rank << ": cord ";
                    p.print_cord();
                    //cout << "DEBUG " << opt::rank << ": m_point ";
                    //p.print_m_point();
                }
            }
        }
        rank++;
        m_comm.barrier();
    }
}

bool LocalOctTree::checkpointReached() const
{
    return m_numReachedCheckpoint == (unsigned) opt::numProc;
}

long computeKey( unsigned int x, unsigned int y, unsigned z)
{
    // number of bits required for each x, y and z = height of the 
    // tree = level_ - 1
    // therefore, height of more than 21 is not supported
    unsigned int height = opt::level - 1;

    int key = 1;
    long x_64 = (long) x << (64 - height);
    long y_64 = (long) y << (64 - height);
    long z_64 = (long) z << (64 - height);
    long mask = 1;
    mask = mask << 63; // leftmost bit is 1, rest 0

    for(unsigned int i = 0; i < height; ++i) {
        key = key << 1;
        if(x_64 & mask) key = key | 1;
        x_64 = x_64 << 1;

        key = key << 1;
        if(y_64 & mask) key = key | 1;
        y_64 = y_64 << 1;

        key = key << 1;
        if(z_64 & mask) key = key | 1;
        z_64 = z_64 << 1;
    } // for

    return key;

}

long LocalOctTree::getCellId(const Point& p)
{
    //return the cell id for a point; 
    //r stands for range 
    double x = p.x - localMinX;
    double y = p.y - localMinY;
    double z = p.z - localMinZ;
    double rx = localMaxX - localMinX;
    double ry = localMaxY - localMinY;
    double rz = localMaxZ - localMinZ;

    double domain = (rx > ry && rx > rz) ?  
        rx :
        ((ry > rz) ? ry : rz);

    unsigned int numCells = 1 << (opt::level - 1);
    double cellSize = domain / numCells;
    int xKey = (unsigned int) (x / cellSize);
    int yKey = (unsigned int) (y / cellSize);
    int zKey = (unsigned int) (z / cellSize);

    return computeKey(xKey, yKey, zKey);
}

void LocalOctTree::setUpCellIds()
{
    //compress all things into cells
    for(unsigned i=0; i < m_data.size(); i++) {
        //m_data --> all the data points on this processor
        int cell_id = getCellId(m_data[i]);
        //TODO: change to user defined type with template
        vector<Cell>::iterator it;
        it = find(m_cells.begin(), m_cells.end(), Cell(cell_id));
        if( it == m_cells.end() ){
            m_cells.push_back(Cell(cell_id, m_data[i]));
            continue;
        }
        //Cell already exists
        it->points.push_back(m_data[i]);
    }
}

bool cmpCell(const Cell &c1, const Cell &c2)
{
    return  c1.cell_id < c2.cell_id;
}

void LocalOctTree::sortLocalCells()
{
    long numTotal = 0;
    numTotal = m_comm.reduce((long)m_cells.size());
    if(opt::rank == 0) 
        cout<< "DEBUG: total " << numTotal <<endl;

    //TODO: extend this to user defined data type
    //So customized iterator/cmp function etc.
    sort(m_cells.begin(), m_cells.end(), cmpCell);
}

void LocalOctTree::gatherAllSamples()
{
    long total  = m_comm.reduce(m_samples.size());
    m_all_samples.resize(total);
    //All proc gather all samples
    for(int i=0;i<opt::numProc && i != opt::rank; i++){
        m_comm.sendSampleAddMessage(i, m_samples);
    }
}

void LocalOctTree::chooseLocalSamples()
{
    int n = m_cells.size();
    int p = opt::numProc;
    int r = opt::rank;
    double step =  (double) n /  sqrt(p);
    int offset = (int) (step * ( (double) r / (double) p));
    int sampleSize = 0;

    cout << "DEBUG: " << r << " n " << n << endl;
    cout << "DEBUG: " << r << " p " << p << endl;
    cout << "DEBUG: " << r << " r " << r << endl;
    cout << "DEBUG: " << r << " step " << step << endl;
    cout << "DEBUG: " << r << " offset " << offset << endl;

    for(double k = offset; int(k) < n; k += step) ++sampleSize;

    m_samples.resize(sampleSize);

    int i = 0;
    double k;
    for(k = offset, i = 0; int(k) < n && i < sampleSize; k += step, ++ i){
        m_samples[i] = m_cells[(int)k];
        //cout << m_samples[i] << endl;
    }

    cout << "DEBUG " << r << ": sample.size(): " << m_samples.size()
        << " sampleSize :" << sampleSize<< endl;
}


void LocalOctTree::printCellIds(string sectionName)
{
    cout <<  "DEBUG " << opt::rank<< ": "<< sectionName << endl;
    for(unsigned int i=0; i<m_cells.size();i++){
        cout << m_cells[i] << " ";
    }
    cout << endl;
}

//
// Run master process state machine
//
void LocalOctTree::runControl()
{
    SetState(NAS_LOADING);
    while(m_state != NAS_DONE){
        switch(m_state){
            case NAS_LOADING:
                {
                    loadPoints();
                    EndState();
                    m_numReachedCheckpoint++;
                    while(!checkpointReached())
                        pumpNetwork();
                    //Load complete
                    SetState(NAS_LOAD_COMPLETE);
                    m_comm.sendControlMessage(APC_SET_STATE,
                            NAS_LOAD_COMPLETE);
                    m_comm.barrier();
                    //printPoints();

                    pumpNetwork();
                    SetState(NAS_SORT);
                    break;
                }
            case NAS_SORT:
                {
                    m_comm.sendControlMessage(APC_SET_STATE,
                            NAS_SORT);
                    m_comm.barrier();
                    setUpGlobalMinMax();
                    setUpCellIds();
                    sortLocalCells();
                    printCellIds(string("nodeId"));
                    chooseLocalSamples();
                    SetState(NAS_DONE);
                    break;
                }
            case NAS_DONE:
                break;
        }
    }
}

//
// Run worker state machine
//
void LocalOctTree::run()
{
    SetState(NAS_LOADING);
    while(m_state != NAS_DONE){
        switch (m_state){
            case NAS_LOADING:
                {
                    loadPoints();
                    EndState();
                    SetState(NAS_WAITING);
                    m_comm.sendCheckPointMessage();
                    break;
                }
            case NAS_LOAD_COMPLETE:
                {
                    m_comm.barrier();
                    //printPoints();

                    pumpNetwork();
                    SetState(NAS_SORT);
                    break;
                }
            case NAS_SORT:
                {
                    m_comm.barrier();
                    setUpGlobalMinMax();
                    setUpCellIds();
                    sortLocalCells();
                    printCellIds(string("nodeId"));
                    chooseLocalSamples();
                    SetState(NAS_DONE);
                    break;
                }
            case NAS_WAITING:
                pumpNetwork();
                break;
            case NAS_DONE:
                break;
        }
    }
}

//should be modified to return a difference
void LocalOctTree::pumpFlush()
{
    pumpNetwork();
    m_comm.flush();
}
