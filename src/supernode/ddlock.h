// Copyright (c) 2018, The Graft Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#pragma once

#include <string>
#include <vector>
#include <iostream>

#include <boost/thread.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/labeled_graph.hpp>



namespace supernode {

using thread_id_t = std::thread::id;

class graft_ddmutex
{
	class ddgraph
	{
	public:
		static ddgraph& get()
		{
			static ddgraph graph;
			return graph;
		}

		ddgraph() = default;
		ddgraph(const ddgraph&) = delete;
		ddgraph& operator=(const ddgraph&) = delete;
		ddgraph(ddgraph&&) = delete;
		ddgraph& operator=(ddgraph&&) = delete;

		void add_node(const thread_id_t& tid) 
		{
			boost::lock_guard<boost::mutex> lg{m_mutex};

			boost::add_vertex(tid, m_graph);
		}

		void add_edge(const thread_id_t& from_tid, const thread_id_t& to_tid, const char *file, int line)
		{
			boost::lock_guard<boost::mutex> lg{m_mutex};

			boost::add_edge_by_label(from_tid, to_tid, m_graph);

			std::vector<int> sccs(boost::num_vertices(m_graph));
			if (boost::connected_components(m_graph, sccs.data()) != 0)
			{
				std::cerr << file << ":" << line << " -- DEADLOCK -- "
							<< from_tid << " -> " << to_tid << "\n";
			}
		}

		void remove_node(const thread_id_t& tid)
		{
			boost::lock_guard<boost::mutex> lg{m_mutex};
			boost::clear_vertex_by_label(tid, m_graph);
		}

	private:
		boost::mutex m_mutex;
		boost::labeled_graph<boost::adjacency_list<boost::vecS,
					boost::vecS, boost::directedS>, thread_id_t> m_graph;
	};

public:
	graft_ddmutex(const char *file, int line) : m_file(file), m_line(line) {}

	void lock()
	{
		thread_id_t ctid = std::this_thread::get_id();
		ddgraph::get().add_node(ctid);

		if (m_owned)
		{
			ddgraph::get().add_edge(m_owner_id, ctid, m_file, m_line);
		}

		m_mutex.lock();
		m_owner_id = ctid;
		m_owned = true;
	}

	void unlock()
	{
		ddgraph::get().remove_node(m_owner_id);
		m_owned = false;
		m_mutex.unlock();
	}

private:
	boost::mutex m_mutex;
	thread_id_t m_owner_id;
	bool m_owned = false;
	const char *m_file;
	int m_line;
};

}

#define GRAFT_DEBUG_LOCK_T supernode::graft_ddmutex
#define GRAFT_DEBUG_LOCK(lock) GRAFT_DEBUG_LOCK_T lock(__FILE__, __LINE__)

#ifdef GRAFT_DEBUG
#define GRAFT_LOCK_T GRAFT_DEBUG_LOCK_T
#define GRAFT_LOCK(lock) GRAFT_DEBUG_LOCK(lock)
#else
#define GRAFT_LOCK_T boost::mutex
#define GRAFT_LOCK(lock) GRAFT_LOCK_T lock
#endif
