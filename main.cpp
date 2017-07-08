#define NDEBUG //for STL

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>
#include <limits>

#include <cstring>
#include <cstdlib>
#include <cctype>

class ArgParser
{
public:
    ArgParser()
        : m_ntop(std::numeric_limits<unsigned int>::max())
        , m_pathIn(NULL)
        , m_pathOut(NULL)
    {}

    bool parse(const int argc, char**argv)
    {
        bool nextNTop = false;
        for (int i=1; i<argc; ++i)
        {
            const char * arg = argv[i];
            if (0 == strcmp(arg, "-n"))
            {
                nextNTop = true;
                continue;
            }
            else
            {
                if (nextNTop)
                {
                    m_ntop = atoi(arg);
                    nextNTop = false;
                }
                else if (NULL == m_pathIn)
                {
                    m_pathIn = arg;
                }
                else
                {
                    m_pathOut = arg;
                }
            }
        }

        return m_pathIn != NULL && m_pathOut != NULL;
    }

    inline unsigned int nTop() const
    {
        return m_ntop;
    }

    inline const char * pathIn() const
    {
        return m_pathIn;
    }

    inline const char * pathOut() const
    {
        return m_pathOut;
    }

private:
    unsigned int m_ntop;
    const char * m_pathIn;
    const char * m_pathOut;
};

class Finder
{
public:
    Finder()
        : isActive()
        , pos(0)
    {
        std::fill(isActive, isActive+templatesCount, true);
    }

    bool gotPrefix(const char c)
    {
        const char * const templates[templatesCount] = {"http://", "https://"};

        bool finded = false;
        for(unsigned int i=0; i<sizeof(templates)/sizeof(char *); ++i)
        {
            isActive[i] = isActive[i] && (templates[i][pos] == c);
            if (isActive[i] && templates[i][pos+1] == '\0')
            {
                pos = 0;
                std::fill(isActive, isActive+templatesCount, true);
                return true;
            }
            finded = finded || isActive[i];
        }

        if (finded)
        {
            ++pos;
        }
        else
        {
            pos = 0;
            std::fill(isActive, isActive+templatesCount, true);
        }

        return false;
    }

private:
    static const int templatesCount = 2;

    bool isActive[templatesCount];
    int pos;
};

class ParserDomainAndPath
{
public:
    ParserDomainAndPath()
        : m_domain()
        , m_path()
        , m_buffer()
    {}

    void reset()
    {
        m_domain.clear();
        m_path.clear();
        m_buffer.clear();
    }

    bool parseDomainAndPath(const char c)
    {
        if (m_domain.empty())
        {
            if(isCorrectForDomain(c))
            {
                m_buffer += c;
            }
            else
            {
                m_domain.swap(m_buffer);
                m_buffer.clear();

                if (isCorrectForPath(c))
                {
                    m_buffer= "/";
                }
                else
                {
                    //Empty path
                    m_path = "/";
                    m_buffer.clear();
                    return true;
                }
            }
        }
        else if (m_path.empty())
        {
            if (isCorrectForPath(c))
            {
                m_buffer += c;
            }
            else
            {
                m_path.swap(m_buffer);
                m_buffer.clear();
                return true;
            }
        }

        return false;
    }

    inline const std::string & domain() const
    {
        return m_domain;
    }

    inline const std::string & path() const
    {
        return m_path;
    }

private:
    bool isCorrectForDomain(const char c) const
    {
        return isdigit(c) != 0
               || isalpha(c) != 0
               || c == '.'
               || c == '-';
    }

    bool isCorrectForPath(const char c) const
    {
        return isdigit(c) != 0
               || isalpha(c) != 0
               || c == '.'
               || c == ','
               || c == '/'
               || c == '+'
               || c == '_';
    }

    std::string m_domain;
    std::string m_path;
    std::string m_buffer;
};

class Information
{
public:
    Information()
        : m_count(1)
        , m_text()
    {}
    explicit Information(const std::string & text)
        : m_count(1)
        , m_text(text)
    {}

    inline bool operator==(const Information & inform) const
    {
        return m_text == inform.m_text;
    }

    inline bool operator<(const Information & inform) const
    {
        return (m_count == inform.m_count) ? m_text < inform.m_text
               : m_count > inform.m_count;
    }

    inline void incCount()
    {
        ++m_count;
    }

    inline unsigned int count() const
    {
        return m_count;
    }

    inline const std::string & text() const
    {
        return m_text;
    }

private:
    unsigned int m_count;
    std::string m_text;
};

class Top
{
public:
    Top()
        :m_topData()
    {}

    void addInTopData(const std::string & name)
    {
        Information data = Information(name);

        std::vector<Information>::iterator it = std::find(m_topData.begin(),
                                                          m_topData.end(),
                                                          data);
        if (it != m_topData.end())
        {
            it->incCount();
        }
        else
        {
            m_topData.push_back(data);
        }
    }

    const std::vector<Information> & formTop()
    {
        std::sort(m_topData.begin(), m_topData.end());
        return m_topData;
    }

private:
    std::vector<Information> m_topData;
};

enum State
{
    FIND_PREFIX,
    GETTING_DATA
};

enum MAIN_ERROR
{
    ERROR_NONE,
    ERROR_PARSE_ARG,
    ERROR_OPEN_INPUT_FILE,
    ERROR_UNDEF_STATE,
    ERROR_OPEN_OUTPUT_FILE
};

int main(int argc, char **argv)
{
    ArgParser argParser;
    if ( !argParser.parse(argc, argv) )
    {
        std::cerr << "Parsing command string failed! Exiting with error ..." << std::endl;
        return ERROR_PARSE_ARG;
    }

    Top domainTop, pathTop;
    unsigned int countUrl = 0;
    std::ifstream in(argParser.pathIn(), std::ifstream::in);
    if (in.is_open())
    {
        char c = '\0';
        State state = FIND_PREFIX;
        Finder prefixFinder;
        ParserDomainAndPath parserData;
        while(in.get(c))
        {
            switch(state)
            {
            case FIND_PREFIX:
                if (prefixFinder.gotPrefix(c))
                {
                    parserData.reset();
                    state = GETTING_DATA;
                }
                break;

            case GETTING_DATA:
                if (parserData.parseDomainAndPath(c))
                {
                    domainTop.addInTopData(parserData.domain());
                    pathTop.addInTopData(parserData.path());
                    ++countUrl;
                    state = FIND_PREFIX;
                }
                break;

            default:
                std::cerr << "Undefined state. Exiting with error..." << std::endl;
                in.close();
                return ERROR_UNDEF_STATE;
            }
        }
        in.close();
    }
    else
    {
        std::cerr << "Opening of input file is failed. Exiting with error..." << std::endl;
        return ERROR_OPEN_INPUT_FILE;
    }

    const std::vector<Information> & domains = domainTop.formTop();
    const std::vector<Information> & paths = pathTop.formTop();
    std::ofstream out(argParser.pathOut(), std::ofstream::out
                                           | std::ofstream::ate);
    if (out.is_open())
    {
        out << "total urls " << countUrl
            << ", domains "  << domains.size()
            << ", paths "    << paths.size() << '\n';

        out << "\ntop domains\n";
        const unsigned int countDomainsInTop = (domains.size() < argParser.nTop())
                                                    ? domains.size()
                                                    : argParser.nTop();
        for(unsigned int i=0; i < countDomainsInTop; ++i)
        {
            out << domains[i].count() << ' '
                << domains[i].text() << '\n';
        }

        out << "\ntop paths\n";
        const int countPathsInTop = (paths.size() < argParser.nTop())
                                              ? paths.size() : argParser.nTop();
        for(int i=0; i < countPathsInTop; ++i)
        {
            out << paths[i].count() << ' '
                << paths[i].text() << '\n';
        }
        out.close();
    }
    else
    {
        std::cerr << "Opening of output file is failed. Exiting with error..." << std::endl;
        return ERROR_OPEN_OUTPUT_FILE;
    }


    return ERROR_NONE;
}
