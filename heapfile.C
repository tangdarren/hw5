//piazza @258 for correct output
//there was an unanswered question in @261 about whether or not we should update the test file so that it follows the piazza post
//i am assuming we should so we uncommeted out some parts

#include "heapfile.h"
#include "error.h"

// routine to create a heapfile
const Status createHeapFile(const string fileName)
{
    File* 		file;
    Status 		status;
    FileHdrPage*	hdrPage;
    int			hdrPageNo;
    int			newPageNo;
    Page*		newPage;

    // try to open the file. This should return an error
    status = db.openFile(fileName, file);
    if (status != OK)
    {
        //provided
        status = db.createFile(fileName);
        if (status != OK) {
            return status;
        } 
        //intializes the file
        status = db.openFile(fileName, file);
        if (status != OK) {
            return status;   
        }




        //FIRST CALL
        status = bufMgr->allocPage(file, hdrPageNo, newPage);
        if (status != OK) {
            return status;
        }
        //Take the Page* pointer returned from allocPage() and cast it to a FileHdrPage*. 
        //Using this pointer initialize the values in the header page.
        hdrPage = (FileHdrPage*) newPage;
        strncpy(hdrPage->fileName, fileName.c_str(), MAXNAMESIZE);
        hdrPage->pageCnt = 1;
        hdrPage->recCnt = 0;




        //SECOND CALL
        status = bufMgr->allocPage(file, newPageNo, newPage);
        if (status != OK)
        {
            //unpin first call if error allocating second
            bufMgr->unPinPage(file, hdrPageNo, true);
            return status;
        }
        //Using the Page* pointer returned, invoke its init() method to initialize the page contents. 
        newPage->init(newPageNo);
        //Finally, store the page number of the data page in firstPage and lastPage attributes of the FileHdrPage.
        hdrPage->firstPage = newPageNo;
        hdrPage->lastPage = newPageNo;
        hdrPage->pageCnt++;




        //When you have done all this unpin both pages and mark them as dirty.
        status = bufMgr->unPinPage(file, hdrPageNo, true);
        if (status != OK){
            return status;
        }
        status = bufMgr->unPinPage(file, newPageNo, true);
        if (status != OK) {
            return status;
        }
        //this took 4 hours to figure out. we did not remember to close the file correctly. this caused us to not be able to delete file 
        //the count is incorrect so the file is never deleted
        status = db.closeFile(file);
        return OK;
    }
    //file exits
    return (FILEEXISTS);
}

// routine to destroy a heapfile
const Status destroyHeapFile(const string fileName)
{
	return (db.destroyFile (fileName));
}

// constructor opens the underlying file
HeapFile::HeapFile(const string & fileName, Status& returnStatus)
{
    Status 	status;
    Page*	pagePtr;

    cout << "opening file " << fileName << endl;

    //I learned you cannot return something in the constructor
    //in heapfile.h there is class definition of heapFile
    //although format in this folder says cerr << to print, i used fprintf because thats what i learned in 537
    //i believe they do the same thing

    // open the file and read in the header page and the first data page
    if ((status = db.openFile(fileName, filePtr)) == OK)
    {

        //get and read the header page
        status = filePtr->getFirstPage(headerPageNo);
        if (status != OK) {
            fprintf(stderr, "failed to get first page (header)\n");
            returnStatus = status;
            return;
        }
        status = bufMgr->readPage(filePtr, headerPageNo, pagePtr);
        if (status != OK) {
            fprintf(stderr, "failed to read header page\n");
            returnStatus = status;
            return;
        }
        headerPage = (FileHdrPage*) pagePtr;
        hdrDirtyFlag = false;



        //get and read the first page
        curPageNo = headerPage->firstPage;
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) {
            fprintf(stderr, "failed to read first data page\n");
            //unpin first call
            bufMgr->unPinPage(filePtr, headerPageNo, false);
            returnStatus = status;
            return;
        }
        curDirtyFlag = false;
        curRec = NULLRID;
        returnStatus = OK;
    }
    else
    {
    	cerr << "open of heap file failed\n";
		returnStatus = status;
		return;
    }
}

// the destructor closes the file
HeapFile::~HeapFile()
{
    Status status;
    cout << "invoking heapfile destructor on file " << headerPage->fileName << endl;

    // see if there is a pinned data page. If so, unpin it 
    if (curPage != NULL)
    {
    	status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
		curPage = NULL;
		curPageNo = 0;
		curDirtyFlag = false;
		if (status != OK) cerr << "error in unpin of date page\n";
    }
	
	// unpin the header page
    status = bufMgr->unPinPage(filePtr, headerPageNo, hdrDirtyFlag);
    if (status != OK) cerr << "error in unpin of header page\n";
	
	// status = bufMgr->flushFile(filePtr);  // make sure all pages of the file are flushed to disk
	// if (status != OK) cerr << "error in flushFile call\n";
	// before close the file
	status = db.closeFile(filePtr);
    if (status != OK)
    {
		cerr << "error in closefile call\n";
		Error e;
		e.print (status);
    }
}

// Return number of records in heap file

const int HeapFile::getRecCnt() const
{
  return headerPage->recCnt;
}

// retrieve an arbitrary record from a file.
// if record is not on the currently pinned page, the current page
// is unpinned and the required page is read into the buffer pool
// and pinned.  returns a pointer to the record via the rec parameter

const Status HeapFile::getRecord(const RID & rid, Record & rec)
{
    Status status;

    //testing statement provided in skeleton code
    // cout<< "getRecord. record (" << rid.pageNo << "." << rid.slotNo << ")" << endl;

    if (curPage == nullptr || curPageNo != rid.pageNo)
    {
        //unpin if necessary
        if (curPage != nullptr)
        {
            status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
            if (status != OK) {
                 return status;
            }
        }

        status = bufMgr->readPage(filePtr, rid.pageNo, curPage);
        if (status != OK) {
            return status;
        }
        curPageNo = rid.pageNo;
        curDirtyFlag = false;
    }

    //get record
    status = curPage->getRecord(rid, rec);
    if (status == OK)
        curRec = rid;

    return status;
    
}

HeapFileScan::HeapFileScan(const string & name,
			   Status & status) : HeapFile(name, status)
{
    filter = NULL;
}

const Status HeapFileScan::startScan(const int offset_,
				     const int length_,
				     const Datatype type_, 
				     const char* filter_,
				     const Operator op_)
{
    if (!filter_) {                        // no filtering requested
        filter = NULL;
        return OK;
    }
    
    if ((offset_ < 0 || length_ < 1) ||
        (type_ != STRING && type_ != INTEGER && type_ != FLOAT) ||
        (type_ == INTEGER && length_ != sizeof(int)
         || type_ == FLOAT && length_ != sizeof(float)) ||
        (op_ != LT && op_ != LTE && op_ != EQ && op_ != GTE && op_ != GT && op_ != NE))
    {
        return BADSCANPARM;
    }

    offset = offset_;
    length = length_;
    type = type_;
    filter = filter_;
    op = op_;

    return OK;
}


const Status HeapFileScan::endScan()
{
    Status status;
    // generally must unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        curPage = NULL;
        curPageNo = 0;
		curDirtyFlag = false;
        return status;
    }
    return OK;
}

HeapFileScan::~HeapFileScan()
{
    endScan();
}

const Status HeapFileScan::markScan()
{
    // make a snapshot of the state of the scan
    markedPageNo = curPageNo;
    markedRec = curRec;
    return OK;
}

const Status HeapFileScan::resetScan()
{
    Status status;
    if (markedPageNo != curPageNo) 
    {
		if (curPage != NULL)
		{
			status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
			if (status != OK) return status;
		}
		// restore curPageNo and curRec values
		curPageNo = markedPageNo;
		curRec = markedRec;
		// then read the page
		status = bufMgr->readPage(filePtr, curPageNo, curPage);
		if (status != OK) return status;
		curDirtyFlag = false; // it will be clean
    }
    else curRec = markedRec;
    return OK;
}


const Status HeapFileScan::scanNext(RID& outRid)
{
    Status  status = OK;
    RID     nextRid;
    RID     tmpRid;
    int     nextPageNo;
    Record  rec;

    //if no pin then first page
    if (curPage == nullptr)
    {
        curPageNo = headerPage->firstPage;
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) {
        
            return status;
        }
        curDirtyFlag = false;
        curRec = NULLRID;
    }

    while (true)
    {
        //get next record 
        //-1 means unitialized
        if (curRec.pageNo == -1 && curRec.slotNo == -1)
        {
            status = curPage->firstRecord(nextRid);
        }
        else
        {
            status = curPage->nextRecord(curRec, nextRid);
        }

        if (status == OK)
        {
            tmpRid = nextRid;
            status = curPage->getRecord(tmpRid, rec);
            if (status != OK) {
                return status;
            }

            if (matchRec(rec))
            {
                curRec = tmpRid;
                outRid = curRec;
                return OK;
            }
            else
            {
                curRec = tmpRid;
            }
        }
        //status errors possible from firstRecord or nextRecord
        else if (status == NORECORDS || status == ENDOFPAGE)
        {
            status = curPage->getNextPage(nextPageNo);
        
            if (nextPageNo <= curPageNo) {
                return FILEEOF;
            }
        
            status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
            if (status != OK) {
                return status;
            }
        
            status = bufMgr->readPage(filePtr, nextPageNo, curPage);
            if (status != OK) {
                return status;
            }

            curPageNo = nextPageNo;
            curDirtyFlag = false;
            curRec = NULLRID;
        }

        //dont know if necessary but just in case i included it
        if (status != OK && status != NORECORDS && status != ENDOFPAGE)
        {
            return status;
        }
    }
}




// returns pointer to the current record.  page is left pinned
// and the scan logic is required to unpin the page 

const Status HeapFileScan::getRecord(Record & rec)
{
    return curPage->getRecord(curRec, rec);
}

// delete record from file. 
const Status HeapFileScan::deleteRecord()
{
    Status status;

    // delete the "current" record from the page
    status = curPage->deleteRecord(curRec);
    curDirtyFlag = true;

    // reduce count of number of records in the file
    headerPage->recCnt--;
    hdrDirtyFlag = true; 
    return status;
}


// mark current page of scan dirty
const Status HeapFileScan::markDirty()
{
    curDirtyFlag = true;
    return OK;
}

const bool HeapFileScan::matchRec(const Record & rec) const
{
    // no filtering requested
    if (!filter) return true;

    // see if offset + length is beyond end of record
    // maybe this should be an error???
    if ((offset + length -1 ) >= rec.length)
	return false;

    float diff = 0;                       // < 0 if attr < fltr
    switch(type) {

    case INTEGER:
        int iattr, ifltr;                 // word-alignment problem possible
        memcpy(&iattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ifltr,
               filter,
               length);
        diff = iattr - ifltr;
        break;

    case FLOAT:
        float fattr, ffltr;               // word-alignment problem possible
        memcpy(&fattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ffltr,
               filter,
               length);
        diff = fattr - ffltr;
        break;

    case STRING:
        diff = strncmp((char *)rec.data + offset,
                       filter,
                       length);
        break;
    }

    switch(op) {
    case LT:  if (diff < 0.0) return true; break;
    case LTE: if (diff <= 0.0) return true; break;
    case EQ:  if (diff == 0.0) return true; break;
    case GTE: if (diff >= 0.0) return true; break;
    case GT:  if (diff > 0.0) return true; break;
    case NE:  if (diff != 0.0) return true; break;
    }

    return false;
}

InsertFileScan::InsertFileScan(const string & name,
                               Status & status) : HeapFile(name, status)
{
  //Do nothing. Heapfile constructor will bread the header page and the first
  // data page of the file into the buffer pool
}

InsertFileScan::~InsertFileScan()
{
    Status status;
    // unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, true);
        curPage = NULL;
        curPageNo = 0;
        if (status != OK) cerr << "error in unpin of data page\n";
    }
}

// Insert a record into the file
const Status InsertFileScan::insertRecord(const Record & rec, RID& outRid)
{
    Page*	newPage;
    int		newPageNo;
    Status	status, unpinstatus;
    RID		rid;
    
    // check for very large records
    if ((unsigned int) rec.length > PAGESIZE-DPFIXED)
    {
        // will never fit on a page, so don't even bother looking
        return INVALIDRECLEN;
    }

    //if current page is null then nothing is pinned
    if (curPage == nullptr)
    {
        curPageNo = headerPage->lastPage;
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) {
            return status;
        }
        curDirtyFlag = false;
    }

    //try to insert
    status = curPage->insertRecord(rec, rid);

    
    //insert in new page
    if (status == NOSPACE)
    {
        status = bufMgr->allocPage(filePtr, newPageNo, newPage);
        if (status != OK)
            return status;

        newPage->init(newPageNo);

        status = newPage->insertRecord(rec, rid);
        if (status != OK)
        {
            bufMgr->unPinPage(filePtr, newPageNo, false);
            return status;
        }

        Page* lastPage;
        status = bufMgr->readPage(filePtr, headerPage->lastPage, lastPage);
        if (status != OK) {
            return status;
        } 

        lastPage->setNextPage(newPageNo);

        status = bufMgr->unPinPage(filePtr, headerPage->lastPage, true);
        if (status != OK) {
            return status;
        }

        headerPage->lastPage = newPageNo;
        headerPage->pageCnt++;
        headerPage->recCnt++;
        //false in constructor by default
        hdrDirtyFlag = true;

        unpinstatus = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        if (unpinstatus != OK) {
            return unpinstatus;
        }

        curPage = newPage;
        curPageNo = newPageNo;
        curDirtyFlag = true;

        outRid = rid;
        return OK;
    }


    //insert in current page
    if (status == OK)
    {
        outRid = rid;

        headerPage->recCnt++;
        hdrDirtyFlag = true;

        curDirtyFlag = true;
        return OK;
    }
    
    if(status != OK && status != NOSPACE)
    {
        return status;
    }
}


