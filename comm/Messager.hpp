#include "MessageBuffer.hpp"
#include "Node.hpp"
#include <map>
#include <unordered_map>
#include <functional>
using namespace std;

template<typename T> class Messager {
    protected:
        typedef std::function<bool (const std::vector<T>&) > predicate_functional;
        typedef std::function<NodeSet(const Node<T>&) > generate_functional;
        typedef std::function<void (std::vector<T>&) > combine_functional;
        typedef std::function<void (std::vector<T>&) > evolve_functional;
        typedef std::function<int (const T&, int) > locate_functional;

        int localBound;

        //messages stored in a list of queues
        MessageBuffer<T> msgBuffer;
        std::vector<T> localBuffer;
        std::vector<Node<T> > localArr;
        std::vector<Node<T> > localStruct;
        std::unordered_map<int, long> nodeTable;

        //> user implementation 
        predicate_functional user_predicate;
        generate_functional user_generate;
        combine_functional user_combine;
        evolve_functional user_evolve;
        locate_functional user_locate;

        unsigned numReachedCheckpoint, checkpointSum, maxLevel;
        NetworkActionState state;

        inline bool _checkpointReached() const {
            return numReachedCheckpoint == numProc;
        }

        virtual void _parseControlMessage(int source) {
            ControlMessage controlMsg = msgBuffer.msgBufferLayer.receiveControlMessage();
            switch (controlMsg.msgType) {
                case APC_SET_STATE:
                    _setState(NetworkActionState(controlMsg.argument));
                    break;
                case APC_CHECKPOINT:
                    numReachedCheckpoint++;
                    checkpointSum += controlMsg.argument;
                    break;
                case APC_WAIT:
                    _setState(NAS_WAITING);
                    msgBuffer.msgBufferLayer.barrier();
                    break;
                case APC_BARRIER:
                    assert(state == NAS_WAITING);
                    msgBuffer.msgBufferLayer.barrier();
                    break;
                    //sl15: need default case
            }
        }

        //sl15: currently the return value is not used, but might be later.

        virtual size_t _pumpNetwork() {
            for (size_t count = 0;; count++) {
                int senderID;
                APMessage msg = msgBuffer.msgBufferLayer.checkMessage(senderID);
                switch (msg) {
                    case APM_CONTROL:
                        {
                            _parseControlMessage(senderID);
                            //deal with control message before 
                            //any other type of message
                            return ++count;
                        }
                    case APM_BUFFERED:
                        {
                            std::queue<Message<T>*> msgs;
                            msgBuffer.msgBufferLayer.receiveBufferedMessage(msgs);
                            while (!msgs.empty()) {
                                Message<T>* p = msgs.front();
                                msgs.pop();
                                //sl15: need to assert that the point is local
                                localBuffer.push_back(p->Data); //push into the local buffer
                            }
                            break;
                        }
                    case APM_NONE:
                        return count;
                }
            }
        }

        inline virtual void _endState() {
            msgBuffer.flush();
        }

        virtual void _setState(NetworkActionState newState) {
            assert(msgBuffer.transmitBufferEmpty());
            state = newState;

            // Reset the checkpoint counter
            numReachedCheckpoint = 0;
            checkpointSum = 0;
        }

        virtual void _sort() = 0;

        virtual void _load(std::string) = 0;

        virtual void _postLoad() = 0;

        virtual void _assemble() = 0;

    private:
        int count = 0;
    public:
        //maxLevel needs to be read in from user input in loadPoint function

        Messager() : numReachedCheckpoint(0), checkpointSum(0), maxLevel(0), state(NAS_WAITING), localBound(0){
        };


        //sl15: this method needs to be overridden by the subclasses
        virtual int computeProcID() {
            return count % numProc;
        }

        //sl15: this function seems to be unnecessary since loading and sorting are all done in run()
        void add(const T& data) {
            int data_id = computeProcID(); //sl15: computeProcID should be implemented by the user
            if (data_id == procRank) localBuffer.push_back(data);
            else msgBuffer.addMessage(data_id, data);
            count++;
        }

        void assign(generate_functional generate_in
                , predicate_functional predicate_in
                , combine_functional combine_in
                , evolve_functional evolve_in
                , locate_functional locate_in) {

            user_generate = generate_in;
            user_predicate = predicate_in;
            user_combine = combine_in;
            user_evolve = evolve_in;
            user_locate = locate_in;
        }

        virtual void build(std::string filename) {
            //_load(filename);
            //_flush_buffer();
            //_assemble();
        }

        virtual void compute() {
            //    for (size_t i = 0; i < localArr.size(); i++) {
            //        localArr[i].genset = user_generate(localArr[i]);
            //    }

            //    msgBuffer.msgBufferLayer.barrier();

            //    for (size_t i = 0; i < localArr.size(); i++) {
            //        std::vector<T > genNodes;
            //        for (auto iter = localArr[i].genset.begin(); iter != localArr[i].genset.end(); iter++) {
            //            genNodes.push_back(localArr[*iter].dataArr[0]);
            //        }
            //        user_combine(genNodes);
            //    }
        }
};
