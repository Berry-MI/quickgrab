//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.service;

import indi.wookat.quickgrab.entity.Buyers;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.entity.Results;
import indi.wookat.quickgrab.mapper.BuyersMapper;
import indi.wookat.quickgrab.mapper.RequestsMapper;
import indi.wookat.quickgrab.mapper.ResultsMapper;
import indi.wookat.quickgrab.util.NetworkUtil;
import jakarta.annotation.Resource;
import java.util.List;
import org.springframework.stereotype.Service;

@Service
public class QueryService {
    @Resource
    private RequestsMapper requestsMapper;
    @Resource
    private ResultsMapper resultsMapper;
    @Resource
    private BuyersMapper buyersMapper;

    public Results getResultsById(Long id) {
        return this.resultsMapper.selectByPrimaryKey(id);
    }

    public void deleteRequestById(Integer id) {
        this.requestsMapper.deleteByPrimaryKey(id);
    }

    public void deleteResultById(Long id) {
        this.resultsMapper.deleteByPrimaryKey(id);
    }

    public boolean checkCookies(String cookies) {
        return NetworkUtil.getNickName(cookies) != null;
    }

    public List<Results> getResultsByFilters(String keyword, String buyerId, Integer type, Integer status, String order, int offset, int limit) {
        String orderColumn;
        String orderDirection;
        switch (order) {
            case "end_time_asc":
                orderColumn = "end_time";
                orderDirection = "ASC";
                break;
            case "end_time_desc":
                orderColumn = "end_time";
                orderDirection = "DESC";
                break;
            default:
                orderColumn = "id";
                orderDirection = "DESC";
        }

        return this.resultsMapper.findResultsByFilters(keyword, buyerId, type, status, orderColumn, orderDirection, offset, limit);
    }

    public List<Requests> getRequestsByFilters(String keyword, String buyerId, Integer type, Integer status, String order, int offset, int limit) {
        String orderColumn;
        String orderDirection;
        switch (order) {
            case "start_time_asc":
                orderColumn = "start_time";
                orderDirection = "ASC";
                break;
            case "start_time_desc":
                orderColumn = "start_time";
                orderDirection = "DESC";
                break;
            default:
                orderColumn = "id";
                orderDirection = "DESC";
        }

        return this.requestsMapper.findRequestsByFilters(keyword, buyerId, type, status, orderColumn, orderDirection, offset, limit);
    }

    public List<Buyers> getAllBuyer() {
        return this.buyersMapper.selectAll();
    }
}
