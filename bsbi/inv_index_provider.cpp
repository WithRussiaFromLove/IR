#include "inv_index_provider.h"
#include "../common/utils.h"
#include "../common/dictionary.h"
#include "postings/posting_list.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace bsbi {

namespace {

std::unordered_map<uint64_t, std::size_t> readOffsets(std::ifstream& ifs) {
    std::unordered_map<uint64_t, std::size_t> offsets;
    uint64_t termId;
    std::size_t offset;
    while (common::serialization::read(ifs, termId) != 0
           && common::serialization::read(ifs, offset) != 0) {
        offsets[termId] = offset;
    }
    return offsets;
}

std::vector<uint64_t> readDocIdList(std::istream& is) {
    std::size_t docsCount;
    common::serialization::read(is, docsCount);

    std::vector<uint64_t> docIdList(docsCount);
    for (int i = 0; i < docsCount; ++i) {
        common::serialization::read(is, docIdList[i]);
        std::string tmpStr;
        common::serialization::read(is, tmpStr);
    }

    std::sort(docIdList.begin(), docIdList.end());
    return docIdList;
}

std::vector<uint64_t> extractDocIds(const postings::PostingList& postingList) {
    const auto& rawPostings = postingList.rawPostings();
    std::vector<uint64_t> docIds(postingList.rawPostings().size());
    for(int i = 0; i < docIds.size(); ++i) {
        docIds[i] = rawPostings[i].docId;
    }
    return docIds;
}

class InvIndexProviderImpl final: public InvIndexProvider {
public:
    explicit InvIndexProviderImpl(const std::filesystem::path& indexDir) {
        common::throwIf(!std::filesystem::exists(indexDir),
            "Specified indexDir does not exist");
        common::throwIf(!std::filesystem::is_directory(indexDir),
            "Specified indexDir is not directory");

        std::filesystem::path postingsPath = indexDir / "postings.bin";
        common::throwIf(!std::filesystem::exists(postingsPath),
            "Can not find postings.bin in indexDir");

        std::filesystem::path offsetsPath = indexDir / "offsets.bin";
        common::throwIf(!std::filesystem::exists(postingsPath),
                        "Can not find offsets.bin in indexDir");

        std::filesystem::path docIndexPath = indexDir / "document_index.bin";
        common::throwIf(!std::filesystem::exists(postingsPath),
                        "Can not find document_index.bin in indexDir");

        std::filesystem::path dictPath = indexDir / "dict.bin";
        common::throwIf(!std::filesystem::exists(postingsPath),
                        "Can not find dict.bin in indexDir");

        postingsIfs_ = std::ifstream(postingsPath);

        std::ifstream offsetsIfs(offsetsPath);
        offsets_ = readOffsets(offsetsIfs);

        std::ifstream docIndexStream(docIndexPath);
        docIdList_ = readDocIdList(docIndexStream);

        std::ifstream dictIfs(dictPath);
        common::serialization::read(dictIfs, dictionary_);
    }

    std::vector<uint64_t> getDocIds(const std::string& term) const override {
        if (!dictionary_.contains(term))
            return {};
        std::cerr << dictionary_.at(term) << std::endl;
        return getDocIdsImpl(dictionary_.at(term));
    }

    std::vector<uint64_t> getDocIds(const uint64_t termId) const override {
        if (!dictionary_.contains(termId))
            return {};
        return getDocIdsImpl(termId);
    }

    const std::vector<uint64_t>& getDocIds() const override {
        return docIdList_;
    }

    virtual ~InvIndexProviderImpl() = default;
private:
    std::vector<uint64_t> getDocIdsImpl(const uint64_t termId) const {
        postingsIfs_.seekg(offsets_.at(termId));

        postings::PostingList postingList;
        postingList.deserialize(postingsIfs_);

        return extractDocIds(postingList);
    }

    mutable std::ifstream postingsIfs_;
    std::unordered_map<uint64_t, std::size_t> offsets_;
    std::vector<uint64_t> docIdList_;
    common::Dictionary dictionary_;
};

}

std::shared_ptr<InvIndexProvider> createInvIndexProvider(const std::filesystem::path& indexDir) {
    return std::make_shared<InvIndexProviderImpl>(indexDir);
}

}