#ifndef CONFIG_MAP_HPP
#define CONFIG_MAP_HPP

#include <map>
#include <string>
#include <stdexcept>

#include "projection.hpp"

//namespace hamcast { namespace util {

class config_map
{

 public:

    typedef std::string                    key_type;
    typedef std::map<key_type,key_type>    mapped_type;
    typedef std::map<key_type,mapped_type> container_type;
    typedef container_type::const_iterator const_iterator;

    /**
     * @throws runtime_error
     */
    void read_ini(const key_type& filename);

    inline bool has_group(const key_type& group) const {
        return m_data.count(group) > 0;
    }

    inline const_iterator begin() const { return m_data.begin(); }

    inline const_iterator end() const { return m_data.end(); }

    inline const_iterator find(const key_type& group) const {
        return m_data.find(group);
    }

    inline const key_type& get(const_iterator group,
                               const key_type& key) const {
        mapped_type::const_iterator j = group->second.find(key);
        if (j != group->second.end()) return j->second;
        return m_empty;
    }

    inline const key_type& get(const key_type& group,
                               const key_type& key) const {
        container_type::const_iterator i = m_data.find(group);
        if (i != m_data.end()) return get(i, key);
        return m_empty;
    }

    template<typename T>
    inline T get_or_else(const key_type& group,
                         const key_type& key,
                         const T& default_value) const {
        auto& str = get(group, key);
        if (!str.empty()) {
            auto tmp = projection<T>(str);
            if (tmp) {
                return *tmp;
            }
        }
        return default_value;
    }

    /**
     * @throws range_error if @p group is unknown
     */
    inline const mapped_type& operator[](const key_type& group) const {
        container_type::const_iterator i = m_data.find(group);
        if (i == m_data.end())
            throw std::range_error("unknown group: " + group);
        return i->second;
    }

 private:

    key_type m_empty;
    container_type m_data;

};

//} } // namespace hamcast::util

#endif // CONFIG_MAP_HPP
