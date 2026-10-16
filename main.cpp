#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

struct Ticket {
    string ticketId;
    string queue;
    string priority;
    string subject;
    string firstResponseMin;
};

struct Match {
    size_t ticketIndex;
    double similarity;
};

using TfIdfVector = map<string, double>;

bool isStopWord(const string& word) {
    static const set<string> stopWords = {
        "a", "an", "the", "to", "my", "for", "with", "on",
        "after", "when", "is", "in", "and", "of", "how",
        "do", "i", "about", "request", "question", "please",
        "help"
    };

    return stopWords.find(word) != stopWords.end();
}

string normalizeSynonym(const string& word) {
    if (word == "access" ||
        word == "log" ||
        word == "login" ||
        word == "signin") {
        return "login";
    }

    if (word == "change" ||
        word == "changed" ||
        word == "changing") {
        return "reset";
    }

    return word;
}

vector<string> tokenizeText(const string& text) {
    vector<string> tokens;
    string word;

    for (char character : text) {
        unsigned char safeCharacter =
            static_cast<unsigned char>(character);

        if (isalnum(safeCharacter)) {
            word += static_cast<char>(tolower(safeCharacter));
        } else if (!word.empty()) {
            if (word.length() > 1 && !isStopWord(word)) {
                tokens.push_back(normalizeSynonym(word));
            }

            word.clear();
        }
    }

    if (word.length() > 1 && !isStopWord(word)) {
        tokens.push_back(normalizeSynonym(word));
    }

    return tokens;
}

TfIdfVector buildTfIdfVector(
    const vector<string>& tokens,
    const map<string, size_t>& documentFrequency,
    size_t documentCount
) {
    TfIdfVector weights;

    if (tokens.empty()) {
        return weights;
    }

    map<string, size_t> wordCounts;

    for (const string& word : tokens) {
        ++wordCounts[word];
    }

    for (const auto& wordEntry : wordCounts) {
        const string& word = wordEntry.first;
        size_t countInText = wordEntry.second;

        double termFrequency =
            static_cast<double>(countInText) / tokens.size();

        size_t documentsWithWord = 0;
        auto frequencyEntry = documentFrequency.find(word);

        if (frequencyEntry != documentFrequency.end()) {
            documentsWithWord = frequencyEntry->second;
        }

        double inverseDocumentFrequency =
            log(
                (documentCount + 1.0) /
                (documentsWithWord + 1.0)
            ) + 1.0;

        weights[word] = termFrequency * inverseDocumentFrequency;
    }

    return weights;
}

double cosineSimilarity(
    const TfIdfVector& first,
    const TfIdfVector& second
) {
    double dotProduct = 0.0;
    double firstLengthSquared = 0.0;
    double secondLengthSquared = 0.0;

    for (const auto& entry : first) {
        double firstWeight = entry.second;
        firstLengthSquared += firstWeight * firstWeight;

        auto matchingWord = second.find(entry.first);

        if (matchingWord != second.end()) {
            dotProduct += firstWeight * matchingWord->second;
        }
    }

    for (const auto& entry : second) {
        double weight = entry.second;
        secondLengthSquared += weight * weight;
    }

    if (firstLengthSquared == 0.0 || secondLengthSquared == 0.0) {
        return 0.0;
    }

    return dotProduct /
        (sqrt(firstLengthSquared) * sqrt(secondLengthSquared));
}

vector<string> parseCsvLine(const string& line) {
    vector<string> fields;
    string field;
    bool insideQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char character = line[i];

        if (character == '"') {
            if (insideQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (character == ',' && !insideQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += character;
        }
    }

    fields.push_back(field);
    return fields;
}

int main() {
    ifstream file("data/support-tickets.csv");

    if (!file) {
        cerr << "Could not open data/support-tickets.csv\n";
        return 1;
    }

    string headerLine;

    if (!getline(file, headerLine)) {
        cerr << "The CSV is empty.\n";
        return 1;
    }

    vector<string> headers = parseCsvLine(headerLine);
    vector<Ticket> tickets;
    string line;
    size_t skippedRows = 0;

    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        vector<string> fields = parseCsvLine(line);

        if (fields.size() != headers.size()) {
            ++skippedRows;
            continue;
        }

        Ticket ticket{
            fields[0],
            fields[3],
            fields[4],
            fields[5],
            fields[6]
        };

        tickets.push_back(ticket);
    }

    size_t urgentCount = 0;

    for (const Ticket& ticket : tickets) {
        if (ticket.priority == "urgent") {
            ++urgentCount;
        }
    }

    map<string, size_t> wordCounts;
    map<string, size_t> documentFrequency;
    vector<vector<string>> tokenizedSubjects;

    for (const Ticket& ticket : tickets) {
        vector<string> tokens = tokenizeText(ticket.subject);

        for (const string& word : tokens) {
            ++wordCounts[word];
        }

        set<string> uniqueWords(tokens.begin(), tokens.end());

        for (const string& word : uniqueWords) {
            ++documentFrequency[word];
        }

        tokenizedSubjects.push_back(tokens);
    }

    vector<TfIdfVector> ticketVectors;

    for (const vector<string>& tokens : tokenizedSubjects) {
        ticketVectors.push_back(
            buildTfIdfVector(tokens, documentFrequency, tickets.size())
        );
    }

    long long totalResponseMinutes = 0;
    size_t ticketsWithResponse = 0;
    size_t ticketsWithoutResponse = 0;

    vector<int> responseTimes;
    map<string, vector<int>> responseTimesByQueue;
    map<string, vector<int>> responseTimesByPriority;
    map<string, size_t> missingResponsesByPriority;

    for (const Ticket& ticket : tickets) {
        vector<int>& priorityTimes = responseTimesByPriority[ticket.priority];

        if (ticket.firstResponseMin.empty()) {
            ++ticketsWithoutResponse;
            ++missingResponsesByPriority[ticket.priority];
            continue;
        }

        int responseMinutes = stoi(ticket.firstResponseMin);

        totalResponseMinutes += responseMinutes;
        ++ticketsWithResponse;

        responseTimes.push_back(responseMinutes);
        responseTimesByQueue[ticket.queue].push_back(responseMinutes);
        priorityTimes.push_back(responseMinutes);
    }

    cout << fixed << setprecision(1);

    cout << "Tickets loaded: " << tickets.size() << '\n';
    cout << "Urgent tickets: " << urgentCount << '\n';
    cout << "Rows skipped: " << skippedRows << '\n';
    cout << "Tickets with a response time: " << ticketsWithResponse << '\n';
    cout << "Tickets without a response time: "
         << ticketsWithoutResponse << '\n';

    if (!responseTimes.empty()) {
        double averageResponseMinutes =
            static_cast<double>(totalResponseMinutes) / ticketsWithResponse;

        sort(responseTimes.begin(), responseTimes.end());

        size_t middleIndex = responseTimes.size() / 2;
        double medianResponseMinutes;

        if (responseTimes.size() % 2 == 1) {
            medianResponseMinutes = responseTimes[middleIndex];
        } else {
            medianResponseMinutes =
                (static_cast<double>(responseTimes[middleIndex - 1]) +
                 responseTimes[middleIndex]) / 2.0;
        }

        cout << "Average first response: "
             << averageResponseMinutes << " minutes\n";
        cout << "Median first response: "
             << medianResponseMinutes << " minutes\n";
    } else {
        cout << "Average first response: unavailable\n";
        cout << "Median first response: unavailable\n";
    }

    cout << "\nResponse time by queue:\n";

    for (auto& queueEntry : responseTimesByQueue) {
        const string& queueName = queueEntry.first;
        vector<int>& queueTimes = queueEntry.second;

        sort(queueTimes.begin(), queueTimes.end());

        long long queueTotalMinutes = 0;

        for (int minutes : queueTimes) {
            queueTotalMinutes += minutes;
        }

        double queueAverage =
            static_cast<double>(queueTotalMinutes) / queueTimes.size();

        size_t middleIndex = queueTimes.size() / 2;
        double queueMedian;

        if (queueTimes.size() % 2 == 1) {
            queueMedian = queueTimes[middleIndex];
        } else {
            queueMedian =
                (static_cast<double>(queueTimes[middleIndex - 1]) +
                 queueTimes[middleIndex]) / 2.0;
        }

        cout << queueName
             << " (" << queueTimes.size() << " responses)"
             << ": average " << queueAverage << " minutes"
             << ", median " << queueMedian << " minutes\n";
    }

    cout << "\nResponse time by priority:\n";

    for (auto& priorityEntry : responseTimesByPriority) {
        const string& priorityName = priorityEntry.first;
        vector<int>& priorityTimes = priorityEntry.second;
        size_t missingCount = missingResponsesByPriority[priorityName];

        if (priorityTimes.empty()) {
            cout << priorityName
                 << ": no recorded response times"
                 << ", missing " << missingCount << '\n';
            continue;
        }

        sort(priorityTimes.begin(), priorityTimes.end());

        long long priorityTotalMinutes = 0;

        for (int minutes : priorityTimes) {
            priorityTotalMinutes += minutes;
        }

        double priorityAverage =
            static_cast<double>(priorityTotalMinutes) / priorityTimes.size();

        size_t middleIndex = priorityTimes.size() / 2;
        double priorityMedian;

        if (priorityTimes.size() % 2 == 1) {
            priorityMedian = priorityTimes[middleIndex];
        } else {
            priorityMedian =
                (static_cast<double>(priorityTimes[middleIndex - 1]) +
                 priorityTimes[middleIndex]) / 2.0;
        }

        cout << priorityName
             << " (" << priorityTimes.size() << " responses)"
             << ": average " << priorityAverage << " minutes"
             << ", median " << priorityMedian << " minutes"
             << ", missing " << missingCount << '\n';
    }

    vector<pair<string, size_t>> rankedWords(
        wordCounts.begin(),
        wordCounts.end()
    );

    sort(
        rankedWords.begin(),
        rankedWords.end(),
        [](const pair<string, size_t>& left,
           const pair<string, size_t>& right) {
            if (left.second != right.second) {
                return left.second > right.second;
            }

            return left.first < right.first;
        }
    );

    cout << "\nMost common subject words:\n";

    size_t wordsToShow = min<size_t>(rankedWords.size(), 10);

    for (size_t i = 0; i < wordsToShow; ++i) {
        cout << i + 1 << ". "
             << rankedWords[i].first
             << " (" << rankedWords[i].second << " occurrences)\n";
    }

    cout << "\nEnter a ticket subject to find similar tickets: ";

    string query;
    getline(cin, query);

    vector<string> queryTokens = tokenizeText(query);

    if (queryTokens.empty()) {
        cout << "No searchable words found in that subject.\n";
        return 0;
    }

    TfIdfVector queryVector =
        buildTfIdfVector(queryTokens, documentFrequency, tickets.size());

    vector<Match> matches;

    for (size_t i = 0; i < tickets.size(); ++i) {
        double score = cosineSimilarity(queryVector, ticketVectors[i]);

        if (score > 0.0) {
            matches.push_back({i, score});
        }
    }

    sort(
        matches.begin(),
        matches.end(),
        [](const Match& left, const Match& right) {
            if (left.similarity != right.similarity) {
                return left.similarity > right.similarity;
            }

            return left.ticketIndex < right.ticketIndex;
        }
    );

    if (matches.empty()) {
        cout << "No similar tickets found.\n";
        return 0;
    }

    cout << "\nTop similar tickets:\n";

    size_t matchesToShow = min<size_t>(matches.size(), 5);

    for (size_t i = 0; i < matchesToShow; ++i) {
        const Match& match = matches[i];
        const Ticket& ticket = tickets[match.ticketIndex];

        cout << i + 1 << ". "
             << ticket.ticketId
             << " | similarity: " << match.similarity * 100.0 << "%"
             << " | queue: " << ticket.queue
             << " | priority: " << ticket.priority
             << " | first response: ";

        if (ticket.firstResponseMin.empty()) {
            cout << "not recorded";
        } else {
            cout << ticket.firstResponseMin << " minutes";
        }

        cout << "\n   Subject: " << ticket.subject << '\n';
    }

    const double minimumSimilarity = 0.50;
    map<string, vector<int>> similarResponseTimesByQueue;
    map<string, size_t> similarTicketsByQueue;
    map<string, size_t> similarMissingByQueue;

    for (const Match& match : matches) {
        if (match.similarity < minimumSimilarity) {
            continue;
        }

        const Ticket& ticket = tickets[match.ticketIndex];
        ++similarTicketsByQueue[ticket.queue];

        if (ticket.firstResponseMin.empty()) {
            ++similarMissingByQueue[ticket.queue];
        } else {
            similarResponseTimesByQueue[ticket.queue].push_back(
                stoi(ticket.firstResponseMin)
            );
        }
    }

    cout << "\nResponse times for similar tickets by queue "
         << "(similarity >= "
         << minimumSimilarity * 100.0 << "%):\n";

    string fastestQueue;
    double lowestMedian = 0.0;
    size_t fastestQueueResponseCount = 0;

    if (similarTicketsByQueue.empty()) {
        cout << "No tickets met the similarity cutoff.\n";
    }

    for (auto& queueEntry : similarTicketsByQueue) {
        const string& queueName = queueEntry.first;
        vector<int>& times = similarResponseTimesByQueue[queueName];
        size_t missingCount = similarMissingByQueue[queueName];

        cout << queueName
             << " (" << queueEntry.second << " similar tickets): ";

        if (times.empty()) {
            cout << "no recorded response times"
                 << ", missing " << missingCount << '\n';
            continue;
        }

        sort(times.begin(), times.end());

        long long totalMinutes = 0;

        for (int minutes : times) {
            totalMinutes += minutes;
        }

        double averageMinutes =
            static_cast<double>(totalMinutes) / times.size();

        size_t middleIndex = times.size() / 2;
        double medianMinutes;

        if (times.size() % 2 == 1) {
            medianMinutes = times[middleIndex];
        } else {
            medianMinutes =
                (static_cast<double>(times[middleIndex - 1]) +
                 times[middleIndex]) / 2.0;
        }

        cout << times.size() << " responses"
             << ": average " << averageMinutes << " minutes"
             << ", median " << medianMinutes << " minutes"
             << ", missing " << missingCount << '\n';

        if (fastestQueue.empty() || medianMinutes < lowestMedian) {
            fastestQueue = queueName;
            lowestMedian = medianMinutes;
            fastestQueueResponseCount = times.size();
        }
    }

    cout << "\nDataset insight:\n";

    if (fastestQueue.empty()) {
        cout << "There are not enough recorded response times "
             << "to compare queues for these similar tickets.\n";
    } else {
        cout << fastestQueue
             << " had the lowest median first-response time among "
             << "the similar tickets: "
             << lowestMedian << " minutes across "
             << fastestQueueResponseCount << " recorded responses.\n";
        cout << "This describes the dataset and is not a guarantee "
             << "for a new ticket.\n";
    }

    return 0;
}