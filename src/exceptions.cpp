#include "exceptions.h"
#include "utility.h"

RuntimeException::RuntimeException(const RuntimeException & rte)
	: m_msg(NULL)
{
	if(rte.m_msg)
		m_msg = strdupnew(rte.m_msg);
}

RuntimeException::RuntimeException(const char* msg)
	: m_msg(NULL)
{
	if(msg)
		m_msg = strdupnew(msg);
}

RuntimeException::~RuntimeException()
{
	delete[] m_msg;
}

const char* RuntimeException::errMsg() const
{
	return m_msg == NULL ? "Error message not set" : m_msg;
}
